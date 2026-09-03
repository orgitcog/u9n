// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimBankTransformProvider.h"
#include "Animation/AnimBoneCompressionCodec.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceDecompressionContext.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "AnimEncoding.h"
#include "ComponentRecreateRenderStateContext.h"
#include "GlobalRenderResources.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "RenderUtils.h"
#include "SceneInterface.h"
#include "ScenePrivate.h"
#include "SceneView.h"
#include "SkeletalRenderPublic.h"
#include "SkinningDefinitions.h"

static FGuid AnimBankGPUProviderId(ANIM_BANK_GPU_TRANSFORM_PROVIDER_GUID);
static FGuid AnimBankCPUProviderId(ANIM_BANK_CPU_TRANSFORM_PROVIDER_GUID);

IMPLEMENT_SCENE_EXTENSION(FAnimBankTransformProvider);

// Animation is always sampled at 30hz
#define ANIM_BANK_SAMPLE_RATE 30.0f

static TAutoConsoleVariable<bool> CVarAnimBankInterp(
	TEXT("r.AnimBank.Interpolation"),
	true,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarAnimBankTimeScale(
	TEXT("r.AnimBank.TimeScale"),
	1.0f,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarAnimBankGPU(
	TEXT("r.AnimBank.GPU"),
	true,
	TEXT(""),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

class FAnimBankEvaluateCS : public FGlobalShader
{
public:
	static constexpr uint32 BonesPerGroup = 64u;

private:
	DECLARE_GLOBAL_SHADER(FAnimBankEvaluateCS);
	SHADER_USE_PARAMETER_STRUCT(FAnimBankEvaluateCS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("BONES_PER_GROUP"), 64);

		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, HeaderBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, BankBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, TransformBuffer)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FAnimBankEvaluateCS, "/Engine/Private/Skinning/AnimBankEval.usf", "BankEvaluateCS", SF_Compute);

class FAnimBankScatterCS : public FGlobalShader
{
public:
	static constexpr uint32 BonesPerGroup = FAnimBankEvaluateCS::BonesPerGroup;

private:
	DECLARE_GLOBAL_SHADER(FAnimBankScatterCS);
	SHADER_USE_PARAMETER_STRUCT(FAnimBankScatterCS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("BONES_PER_GROUP"), 64);

		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, HeaderBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, SrcTransformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, TransformBuffer)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FAnimBankScatterCS, "/Engine/Private/Skinning/AnimBankEval.usf", "BankScatterCS", SF_Compute);

bool FAnimBankTransformProvider::ShouldCreateExtension(FScene& InScene)
{
	return NaniteSkinnedMeshesSupported() && DoesRuntimeSupportNanite(GetFeatureLevelShaderPlatform(InScene.GetFeatureLevel()), true, true);
}

void FAnimBankTransformProvider::InitExtension(FScene& InScene)
{
	if (auto TransformProvider = InScene.GetExtensionPtr<FSkinningTransformProvider>())
	{
		// Register GPU animation bank transform provider
		TransformProvider->RegisterProvider(
			AnimBankGPUProviderId,
			FSkinningTransformProvider::FOnProvideTransforms::CreateRaw(this, &FAnimBankTransformProvider::ProvideGPUBankTransforms)
		);

		// Register CPU animation bank transform provider
		TransformProvider->RegisterProvider(
			AnimBankCPUProviderId,
			FSkinningTransformProvider::FOnProvideTransforms::CreateRaw(this, &FAnimBankTransformProvider::ProvideCPUBankTransforms)
		);
	}
}

FAnimBankRecordHandle FAnimBankTransformProvider::RegisterBank(const FAnimBankDesc& Desc)
{
	FAnimBankRecordHandle Handle;

	const FAnimBankDesc::FDescHash DescHash = BankRecordMap.ComputeHash(Desc);
	FAnimBankRecord::FRecordId RecordId = BankRecordMap.FindOrAddIdByHash(DescHash, Desc, FAnimBankRecord());

	Handle.Id	= RecordId.GetIndex();
	Handle.Hash	= DescHash.AsUInt();

	FAnimBankRecord& BankRecord = BankRecordMap.GetByElementId(RecordId).Value;
	if (BankRecord.ReferenceCount == 0)
	{
		// First reference
		BankRecord.Desc = Desc;
		BankRecord.RecordId = Handle.Id;

		// Calculate total number of sequence rot/pos keys
		const FAnimBankData& BankData = Desc.BankAsset.Get()->GetData();

		check(Desc.SequenceIndex < uint32(BankData.Entries.Num()));
		const FAnimBankEntry& BankEntry = BankData.Entries[Desc.SequenceIndex];

		const float SampleInterval = 1.0f / ANIM_BANK_SAMPLE_RATE;
		const float TrackLength = float(BankEntry.FrameCount - 1) * SampleInterval;

		BankRecord.KeyOffset	= BankAllocator.Allocate(BankEntry.KeyCount);
		BankRecord.KeyCount		= BankEntry.KeyCount;
		BankRecord.CurrentTime	= FMath::Clamp<float>(BankEntry.Position, 0.0f, TrackLength);
		BankRecord.PreviousTime = BankRecord.CurrentTime;
		BankRecord.Playing		= BankEntry.IsAutoStart() ? 1 : 0;
		BankRecord.FrameCount	= BankEntry.FrameCount;
		BankRecord.PositionKeys = BankEntry.PositionKeys; // TODO: Avoid copy
		BankRecord.RotationKeys = BankEntry.RotationKeys; // TODO: Avoid copy
		BankRecord.AssetMapping = BankData.Mapping; // TODO: Avoid copy

		check(BankEntry.KeyCount == BankRecord.PositionKeys.Num() && BankEntry.KeyCount == BankRecord.RotationKeys.Num());
	}

	++BankRecord.ReferenceCount;
	return Handle;
}

void FAnimBankTransformProvider::UnregisterBank(const FAnimBankRecordHandle& Handle)
{
	FAnimBankRecord::FRecordId RecordId(Handle.Id);
	check(RecordId.IsValid());

	FAnimBankRecord& BankRecord = BankRecordMap.GetByElementId(RecordId).Value;
	check(BankRecord.ReferenceCount > 0);
	--BankRecord.ReferenceCount;

	if (BankRecord.ReferenceCount == 0)
	{
		BankAllocator.Free(BankRecord.KeyOffset, BankRecord.KeyCount);
		BankRecordMap.RemoveByElementId(RecordId);
	}
}

struct FAnimBankGPUData
{
	TArray<uint32, SceneRenderingAllocator> IdToOffsetMapping;

	FRDGBufferRef BoneBlockBuffer	= nullptr;
	FRDGBufferRef BankDataBuffer	= nullptr;
	FRDGBufferRef TransformBuffer	= nullptr;

	uint32 RecordCount = 0;
	uint32 TransformCount = 0;
	uint32 BlockCount = 0;
	uint32 KeyCount = 0;

	bool bValid = false;
};

struct FAnimBankCPUData
{
	TArray<uint32, SceneRenderingAllocator> IdToOffsetMapping;

	FRDGBufferRef TransformBuffer = nullptr;

	uint32 TransformCount = 0;
	uint32 RecordCount = 0;

	bool bValid = false;
};

static FAnimBankGPUData BuildAnimBankGPUData(const FAnimBankRecordMap& BankRecordMap, FRDGBuilder& GraphBuilder)
{
	const uint32 BonesPerGroup = FAnimBankEvaluateCS::BonesPerGroup;

	FAnimBankGPUData BankData{};

	int32 MaxOffsetIndex = INDEX_NONE;

	for (const auto& RecordPair : BankRecordMap)
	{
		const FAnimBankRecord& BankRecord = RecordPair.Value;
		check(BankRecord.PositionKeys.Num() == BankRecord.RotationKeys.Num());

		MaxOffsetIndex = FMath::Max<int32>(MaxOffsetIndex, BankRecord.RecordId);

		if (BankRecord.PositionKeys.Num() == 0 || !BankRecord.Playing)
		{
			continue;
		}

		++BankData.RecordCount;

		const FAnimBankData& AnimBankData = BankRecord.Desc.BankAsset->GetData();
		const uint32 BoneCount = AnimBankData.Mapping.BoneCount;

		BankData.TransformCount += BoneCount;
		BankData.BlockCount += FMath::DivideAndRoundUp(BoneCount, BonesPerGroup);
		BankData.KeyCount += BankRecord.PositionKeys.Num();
	}

	BankData.IdToOffsetMapping.SetNumZeroed(MaxOffsetIndex + 1);

	BankData.bValid = BankData.BlockCount > 0;
	if (BankData.bValid)
	{
		const bool bInterpolating = CVarAnimBankInterp.GetValueOnRenderThread();

		// Transform Buffer
		{
			const uint32 TransformSize = sizeof(FCompressedBoneTransform) * BankData.TransformCount;
			BankData.TransformBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc(TransformSize), TEXT("AnimBank.Transforms"));
		}

		// Bank Record Data
		const uint32 BankHeaderSize = sizeof(UE::HLSL::FBankRecordHeader) * BankData.RecordCount;
		const uint32 BankMappingSize = sizeof(float) * 7 * BankData.TransformCount;
		const uint32 BankKeySize = sizeof(float) * 7 * BankData.KeyCount;
		const uint32 BankDataSize = BankHeaderSize + BankMappingSize + BankKeySize;
		uint8* BankRecordData = GraphBuilder.AllocPODArray<uint8>(BankDataSize);

		// Bone Block Headers
		UE::HLSL::FBankBlockHeader* BlockHeaders = GraphBuilder.AllocPODArray<UE::HLSL::FBankBlockHeader>(BankData.BlockCount);

		// Construct Headers and Indirections
		{
			uint32 BlockWrite = 0;
			uint32 TransformOffset = 0;

			uint8* BankRecordWrite = BankRecordData;

			for (auto& RecordPair : BankRecordMap)
			{
				const FAnimBankRecord& BankRecord = RecordPair.Value;
				if (BankRecord.KeyCount == 0 || !BankRecord.Playing)
				{
					continue;
				}

				const FAnimBankData& AnimBankData = BankRecord.Desc.BankAsset->GetData();
				const uint32 BoneCount = AnimBankData.Mapping.BoneCount;

				const uint32 BankRecordOffset = UE_PTRDIFF_TO_UINT32(BankRecordWrite - BankRecordData);

				UE::HLSL::FBankRecordHeader& Header = *reinterpret_cast<UE::HLSL::FBankRecordHeader*>(BankRecordWrite);
				BankRecordWrite += sizeof(UE::HLSL::FBankRecordHeader);

				Header.BoneCount		= BoneCount;
				Header.FrameCount		= BankRecord.FrameCount;
				Header.SampleRate		= ANIM_BANK_SAMPLE_RATE;
				Header.PlayRate			= BankRecord.Desc.PlayRate;
				Header.CurrentTime		= BankRecord.CurrentTime;
				Header.PreviousTime		= BankRecord.PreviousTime;
				Header.TransformOffset	= TransformOffset;
				Header.Playing			= BankRecord.Playing;
				Header.Interpolating	= bInterpolating;
				Header.HasScale			= false;
				checkSlow(Header.TransformOffset == TransformOffset);

				BankData.IdToOffsetMapping[BankRecord.RecordId] = Header.TransformOffset;
				
				TransformOffset += sizeof(FCompressedBoneTransform) * Header.BoneCount;

				// Full bone blocks
				uint32 BlockTransformOffset = Header.TransformOffset;
				const uint32 FullBlockCount = Header.BoneCount / BonesPerGroup;
				for (uint32 BlockIndex = 0; BlockIndex < FullBlockCount; ++BlockIndex)
				{
					BlockHeaders[BlockWrite].BlockLocalIndex		= BlockIndex;
					BlockHeaders[BlockWrite].BlockBoneCount			= BonesPerGroup;
					BlockHeaders[BlockWrite].BlockTransformOffset	= BlockTransformOffset;
					BlockHeaders[BlockWrite].BankRecordOffset		= BankRecordOffset;
					++BlockWrite;

					BlockTransformOffset += (sizeof(FCompressedBoneTransform) * BonesPerGroup);
				}

				// Partial bone block (remainder)
				const uint32 PartialBoneCount = Header.BoneCount - (FullBlockCount * BonesPerGroup);
				if (PartialBoneCount > 0)
				{
					BlockHeaders[BlockWrite].BlockLocalIndex		= FullBlockCount;
					BlockHeaders[BlockWrite].BlockBoneCount			= PartialBoneCount;
					BlockHeaders[BlockWrite].BlockTransformOffset	= BlockTransformOffset;
					BlockHeaders[BlockWrite].BankRecordOffset		= BankRecordOffset;
					++BlockWrite;
				}

				// Asset Mapping
				{
					const FVector3f* PositionKeys	= BankRecord.AssetMapping.PositionKeys.GetData();
					const FQuat4f* RotationKeys		= BankRecord.AssetMapping.RotationKeys.GetData();

					for (uint32 KeyIndex = 0; KeyIndex < Header.BoneCount; ++KeyIndex)
					{
						float* KeyWrite = reinterpret_cast<float*>(BankRecordWrite);

						KeyWrite[0] = RotationKeys[KeyIndex].X;
						KeyWrite[1] = RotationKeys[KeyIndex].Y;
						KeyWrite[2] = RotationKeys[KeyIndex].Z;
						KeyWrite[3] = RotationKeys[KeyIndex].W;

						KeyWrite[4] = PositionKeys[KeyIndex].X;
						KeyWrite[5] = PositionKeys[KeyIndex].Y;
						KeyWrite[6] = PositionKeys[KeyIndex].Z;

						BankRecordWrite += sizeof(float) * 7;
					}
				}

				// Position and Rotation Keys
				{
					const FVector3f* PositionKeys	= BankRecord.PositionKeys.GetData();
					const FQuat4f* RotationKeys		= BankRecord.RotationKeys.GetData();

					for (uint32 KeyIndex = 0; KeyIndex < BankRecord.KeyCount; ++KeyIndex)
					{
						float* KeyWrite = reinterpret_cast<float*>(BankRecordWrite);

						KeyWrite[0] = RotationKeys[KeyIndex].X;
						KeyWrite[1] = RotationKeys[KeyIndex].Y;
						KeyWrite[2] = RotationKeys[KeyIndex].Z;
						KeyWrite[3] = RotationKeys[KeyIndex].W;

						KeyWrite[4] = PositionKeys[KeyIndex].X;
						KeyWrite[5] = PositionKeys[KeyIndex].Y;
						KeyWrite[6] = PositionKeys[KeyIndex].Z;

						BankRecordWrite += sizeof(float) * 7;
					}
				}
				
			}
		
			check(UE_PTRDIFF_TO_UINT32(BankRecordWrite - BankRecordData) == BankDataSize);
		}

		// Create and upload buffers

		BankData.BoneBlockBuffer = CreateByteAddressBuffer(
			GraphBuilder,
			TEXT("AnimBank.BlockHeaders"),
			FMath::RoundUpToPowerOfTwo(sizeof(UE::HLSL::FBankBlockHeader) * BankData.BlockCount),
			BlockHeaders,
			sizeof(UE::HLSL::FBankBlockHeader) * BankData.BlockCount,
			// The buffer data is allocated above on the RDG timeline
			ERDGInitialDataFlags::NoCopy
		);

		BankData.BankDataBuffer = CreateByteAddressBuffer(
			GraphBuilder,
			TEXT("AnimBank.BankData"),
			FMath::RoundUpToPowerOfTwo(BankDataSize),
			BankRecordData,
			BankDataSize,
			// The buffer data is allocated above on the RDG timeline
			ERDGInitialDataFlags::NoCopy
		);
	}

	return BankData;
}

static FAnimBankCPUData BuildAnimBankCPUData(const FAnimBankRecordMap& BankRecordMap, FRDGBuilder& GraphBuilder)
{
	FAnimBankCPUData BankData{};

	int32 MaxOffsetIndex = INDEX_NONE;

	for (const auto& RecordPair : BankRecordMap)
	{
		const FAnimBankRecord& BankRecord = RecordPair.Value;
		check(BankRecord.PositionKeys.Num() == BankRecord.RotationKeys.Num());

		MaxOffsetIndex = FMath::Max<int32>(MaxOffsetIndex, BankRecord.RecordId);

		if (BankRecord.PositionKeys.Num() == 0 || !BankRecord.Playing)
		{
			continue;
		}

		const FAnimBankData& AnimBankData = BankRecord.Desc.BankAsset->GetData();
		const uint32 BoneCount = AnimBankData.Mapping.BoneCount;

		++BankData.RecordCount;

		BankData.TransformCount += BoneCount;
	}

	BankData.IdToOffsetMapping.SetNumZeroed(MaxOffsetIndex + 1);

	for (auto& RecordPair : BankRecordMap)
	{
		const FAnimBankRecord& BankRecord = RecordPair.Value;
		BankData.IdToOffsetMapping[BankRecord.RecordId] = ~uint32(0);
	}

	BankData.bValid = BankData.RecordCount > 0;
	if (BankData.bValid)
	{
		const bool bInterpolating = CVarAnimBankInterp.GetValueOnRenderThread();

		FCompressedBoneTransform* Transforms = GraphBuilder.AllocPODArray<FCompressedBoneTransform>(BankData.TransformCount);

		uint32 TransformOffset = 0;

		for (const auto& RecordPair : BankRecordMap)
		{
			const FAnimBankRecord& BankRecord = RecordPair.Value;
			if (BankRecord.PositionKeys.Num() == 0 || !BankRecord.Playing)
			{
				continue;
			}

			check(BankRecord.PositionKeys.Num() == BankRecord.RotationKeys.Num());

			BankData.IdToOffsetMapping[BankRecord.RecordId] = (TransformOffset * sizeof(FCompressedBoneTransform));

			const float SampleInterval = 1.0f / ANIM_BANK_SAMPLE_RATE;
			const float TrackLength = float(BankRecord.FrameCount - 1) * SampleInterval;

			double Time = (double)FMath::Fmod(BankRecord.CurrentTime, TrackLength);
			if (Time < 0.0)
			{
				Time += TrackLength;
			}

			int32 KeyIndex0, KeyIndex1;
			float Alpha;
			FAnimationRuntime::GetKeyIndicesFromTime(KeyIndex0, KeyIndex1, Alpha, Time, BankRecord.FrameCount, TrackLength);

			if (!bInterpolating)
			{
				// Forcing alpha to zero disables pose interpolation (interpolation method is "step")
				Alpha = 0.0f;
			}

			const FAnimBankData& AnimBankData = BankRecord.Desc.BankAsset->GetData();
			const uint32 BoneCount = AnimBankData.Mapping.BoneCount;

			for (uint32 TransformIndex = 0; TransformIndex < BoneCount; ++TransformIndex)
			{
				const FTransform InvGlobalRefPoseXform = FTransform(FQuat(BankRecord.AssetMapping.RotationKeys[TransformIndex]), FVector(BankRecord.AssetMapping.PositionKeys[TransformIndex]));

				FTransform MeshGlobalAnimPoseXform;

				if (Alpha <= 0.f)
				{
					MeshGlobalAnimPoseXform = FTransform(FQuat(BankRecord.RotationKeys[(KeyIndex0 * BoneCount) + TransformIndex]), FVector(BankRecord.PositionKeys[(KeyIndex0 * BoneCount) + TransformIndex]));
					MeshGlobalAnimPoseXform.NormalizeRotation();
				}
				else if (Alpha >= 1.f)
				{
					MeshGlobalAnimPoseXform = FTransform(FQuat(BankRecord.RotationKeys[(KeyIndex1 * BoneCount) + TransformIndex]), FVector(BankRecord.PositionKeys[(KeyIndex1 * BoneCount) + TransformIndex]));
					MeshGlobalAnimPoseXform.NormalizeRotation();
				}
				else
				{
					FTransform MeshGlobalXformA = FTransform(FQuat(BankRecord.RotationKeys[(KeyIndex0 * BoneCount) + TransformIndex]), FVector(BankRecord.PositionKeys[(KeyIndex0 * BoneCount) + TransformIndex]));
					FTransform MeshGlobalXformB = FTransform(FQuat(BankRecord.RotationKeys[(KeyIndex1 * BoneCount) + TransformIndex]), FVector(BankRecord.PositionKeys[(KeyIndex1 * BoneCount) + TransformIndex]));

					// Ensure rotations are normalized
					MeshGlobalXformA.NormalizeRotation();
					MeshGlobalXformB.NormalizeRotation();

					MeshGlobalAnimPoseXform.Blend(MeshGlobalXformA, MeshGlobalXformB, Alpha);
					MeshGlobalAnimPoseXform.NormalizeRotation();
				}

				MeshGlobalAnimPoseXform = InvGlobalRefPoseXform * MeshGlobalAnimPoseXform;

				FMatrix44f Transform = (FMatrix44f)MeshGlobalAnimPoseXform.ToMatrixNoScale();

				StoreCompressedBoneTransform(&Transforms[TransformOffset], Transform);
				++TransformOffset;
			}
		}

		// Transform Buffer
		{
			const uint32 TransformSize = sizeof(FCompressedBoneTransform) * BankData.TransformCount;
			BankData.TransformBuffer = CreateByteAddressBuffer(
				GraphBuilder,
				TEXT("AnimBank.Transforms"),
				TransformSize,
				Transforms,
				TransformSize,
				// The buffer data is allocated above on the RDG timeline
				ERDGInitialDataFlags::NoCopy
			);
		}
	}

	return BankData;
}

void FAnimBankTransformProvider::AdvanceAnimation(FSkinningTransformProvider::FProviderContext& Context)
{
	const float GlobalTimeScale = CVarAnimBankTimeScale.GetValueOnRenderThread();

	for (auto& RecordPair : BankRecordMap)
	{
		FAnimBankRecord& BankRecord = RecordPair.Value;
		if (!BankRecord.Playing)
		{
			continue;
		}

		BankRecord.PreviousTime = BankRecord.CurrentTime;
		BankRecord.CurrentTime += (Context.DeltaTime * BankRecord.Desc.PlayRate) * GlobalTimeScale;
	}
}

void FAnimBankTransformProvider::ScatterAnimation(
	FSkinningTransformProvider::FProviderContext& Context,
	const TConstArrayView<uint32> IdToOffsetMapping,
	FRDGBufferRef TransformBuffer
)
{
	FRDGBuilder& GraphBuilder = Context.GraphBuilder;

	const uint32 BonesPerGroup = FAnimBankScatterCS::BonesPerGroup;

	uint32 BlockCount = 0;
	for (const FUintVector2& Indirection : Context.Indirections)
	{
		const FPrimitiveSceneInfo* Primitive = Context.Primitives[Indirection.X];
		auto* SkinnedProxy = static_cast<Nanite::FSkinnedSceneProxy*>(Primitive->Proxy);
		const uint32 TransformCount = SkinnedProxy->GetMaxBoneTransformCount();
		const uint32 AnimationCount = SkinnedProxy->GetUniqueAnimationCount();
		BlockCount += FMath::DivideAndRoundUp(TransformCount, BonesPerGroup) * AnimationCount;
	}

	UE::HLSL::FBankScatterHeader* BlockHeaders = GraphBuilder.AllocPODArray<UE::HLSL::FBankScatterHeader>(BlockCount);

	uint32 BlockWrite = 0;
	for (const FUintVector2& Indirection : Context.Indirections)
	{
		const FPrimitiveSceneInfo* Primitive = Context.Primitives[Indirection.X];
		auto* SkinnedProxy = static_cast<Nanite::FSkinnedSceneProxy*>(Primitive->Proxy);
		const uint32 TransformCount = SkinnedProxy->GetMaxBoneTransformCount();
		const uint32 AnimationCount = SkinnedProxy->GetUniqueAnimationCount();

		bool bIsValid = false;
		const TConstArrayView<uint64> BankIds = SkinnedProxy->GetAnimationProviderData(bIsValid);
		check(bIsValid && BankIds.Num() == SkinnedProxy->GetUniqueAnimationCount());

		const uint32 FullBlockCount = TransformCount / BonesPerGroup;
		const uint32 PartialTransformCount = TransformCount - (FullBlockCount * BonesPerGroup);

		uint32 DstTransformOffset = Indirection.Y;

		for (const uint64 BankId : BankIds)
		{
			if (BankId == ~uint64(0))
			{
				continue;
			}

			check(BankId < uint64(IdToOffsetMapping.Num()));
			uint32 SrcTransformOffset = IdToOffsetMapping[int32(BankId)];

			const bool bIsPending = (SrcTransformOffset == ~uint32(0));
			const uint32 BlockStart = BlockWrite;

			for (uint32 BlockIndex = 0; BlockIndex < FullBlockCount; ++BlockIndex)
			{
				BlockHeaders[BlockWrite].BlockLocalIndex = BlockIndex;
				BlockHeaders[BlockWrite].BlockSrcTransformOffset = SrcTransformOffset;
				BlockHeaders[BlockWrite].BlockDstTransformOffset = DstTransformOffset;
				BlockHeaders[BlockWrite].BlockTransformCount = BonesPerGroup;
				BlockHeaders[BlockWrite].TotalTransformCount = TransformCount;
				++BlockWrite;

				SrcTransformOffset += (BonesPerGroup * sizeof(FCompressedBoneTransform));
				DstTransformOffset += (BonesPerGroup * sizeof(FCompressedBoneTransform));
			}

			if (PartialTransformCount > 0)
			{
				BlockHeaders[BlockWrite].BlockLocalIndex = FullBlockCount;
				BlockHeaders[BlockWrite].BlockSrcTransformOffset = SrcTransformOffset;
				BlockHeaders[BlockWrite].BlockDstTransformOffset = DstTransformOffset;
				BlockHeaders[BlockWrite].BlockTransformCount = PartialTransformCount;
				BlockHeaders[BlockWrite].TotalTransformCount = TransformCount;
				++BlockWrite;

				SrcTransformOffset += (PartialTransformCount * sizeof(FCompressedBoneTransform));
				DstTransformOffset += (PartialTransformCount * sizeof(FCompressedBoneTransform));
			}

			if (bIsPending)
			{
				for (uint32 BlockIndex = 0; BlockIndex < FullBlockCount; ++BlockIndex)
				{
					BlockHeaders[BlockStart + BlockIndex].BlockSrcTransformOffset = ~uint32(0);
				}

				if (PartialTransformCount > 0)
				{
					BlockHeaders[BlockStart + FullBlockCount].BlockSrcTransformOffset = ~uint32(0);
				}
			}

			// Advance past all the previous transforms as well
			DstTransformOffset += (sizeof(FCompressedBoneTransform) * TransformCount);
		}
	}

	FRDGBufferRef ScatterBlockHeaders = CreateByteAddressBuffer(
		GraphBuilder,
		TEXT("AnimBank.ScatterHeaders"),
		FMath::RoundUpToPowerOfTwo(sizeof(UE::HLSL::FBankScatterHeader) * BlockCount),
		BlockHeaders,
		sizeof(UE::HLSL::FBankScatterHeader) * BlockCount,
		// The buffer data is allocated above on the RDG timeline
		ERDGInitialDataFlags::NoCopy
	);

	// Can be null if there are banks, but all are pending (so evaluation is skipped)
	// None of the blocks will be sourcing from this buffer, ref pose will be written
	FRDGBufferRef SrcTransformBuffer = TransformBuffer ? TransformBuffer : GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 8u);

	FAnimBankScatterCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FAnimBankScatterCS::FParameters>();
	PassParameters->TransformBuffer = GraphBuilder.CreateUAV(Context.TransformBuffer);
	PassParameters->SrcTransformBuffer = GraphBuilder.CreateSRV(SrcTransformBuffer);
	PassParameters->HeaderBuffer = GraphBuilder.CreateSRV(ScatterBlockHeaders);

	auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FAnimBankScatterCS>();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("AnimBankScatter"),
		ComputeShader,
		PassParameters,
		FIntVector(BlockCount, 1, 1)
	);
}

void FAnimBankTransformProvider::ProvideGPUBankTransforms(FSkinningTransformProvider::FProviderContext& Context)
{
	AdvanceAnimation(Context);

	FRDGBuilder& GraphBuilder = Context.GraphBuilder;
	const FAnimBankGPUData BankData = BuildAnimBankGPUData(BankRecordMap, GraphBuilder);
	if (BankData.bValid)
	{
		// Evaluate animation banks
		{
			FAnimBankEvaluateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FAnimBankEvaluateCS::FParameters>();
			PassParameters->TransformBuffer = GraphBuilder.CreateUAV(BankData.TransformBuffer);
			PassParameters->BankBuffer = GraphBuilder.CreateSRV(BankData.BankDataBuffer);
			PassParameters->HeaderBuffer = GraphBuilder.CreateSRV(BankData.BoneBlockBuffer);

			auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FAnimBankEvaluateCS>();
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("AnimBankEvaluate"),
				ComputeShader,
				PassParameters,
				FIntVector(BankData.BlockCount, 1, 1)
			);
		}
	}

	// Scatter animation bank results
	ScatterAnimation(Context, MakeConstArrayView(BankData.IdToOffsetMapping), BankData.TransformBuffer);
}

void FAnimBankTransformProvider::ProvideCPUBankTransforms(FSkinningTransformProvider::FProviderContext& Context)
{
	AdvanceAnimation(Context);

	// Evaluate animation banks on the CPU, and then upload
	// the results to the GPU as input to the scatter.

	FRDGBuilder& GraphBuilder = Context.GraphBuilder;
	const FAnimBankCPUData BankData = BuildAnimBankCPUData(BankRecordMap, GraphBuilder);

	// Scatter animation bank results
	ScatterAnimation(Context, MakeConstArrayView(BankData.IdToOffsetMapping), BankData.TransformBuffer);
}

const FSkinningTransformProvider::FProviderId& GetAnimBankProviderId()
{
	if (CVarAnimBankGPU.GetValueOnRenderThread())
	{
		return AnimBankGPUProviderId;
	}
	else
	{
		return AnimBankCPUProviderId;
	}
}