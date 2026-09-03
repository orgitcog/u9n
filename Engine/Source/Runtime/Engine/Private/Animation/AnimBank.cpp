// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimBank.h"
#include "Animation/AnimBank.h"
#include "Animation/AnimBoneCompressionCodec.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceDecompressionContext.h"
#include "AnimationRuntime.h"
#include "AnimationUtils.h"
#include "AnimEncoding.h"
#include "Async/ParallelFor.h"
#include "DerivedDataCacheInterface.h"
#include "EngineUtils.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Serialization/ArchiveCrc32.h"
#include "Logging/MessageLog.h"
#include "Misc/App.h"
#include "ReferenceSkeleton.h"
#include "RenderUtils.h"
#include "SkeletalRenderPublic.h"
#include "UObject/FrameworkObjectVersion.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UObjectThreadContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimBank)

#if WITH_EDITOR
#include "Animation/AnimationSequenceCompiler.h"
#include "Animation/AnimBankCompiler.h"
#include "Async/AsyncWork.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "Experimental/Misc/ExecutionResource.h"
#include "Serialization/MemoryHasher.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#endif

#include "Components/InstancedSkinnedMeshComponent.h"

#define LOCTEXT_NAMESPACE "AnimBank"

CSV_DECLARE_CATEGORY_MODULE_EXTERN(ENGINE_API, Animation);

DEFINE_LOG_CATEGORY(LogAnimBank);

#if WITH_EDITOR

UAnimBank::FOnDependenciesChanged UAnimBank::OnDependenciesChanged;

static inline void DecomposeTransform(const FTransform& Transform, FVector3f& OutTranslation, FQuat4f& OutRotation)
{
	// Get Translation
	OutTranslation = (FVector3f)Transform.GetTranslation();

	// Get Rotation 
	OutRotation = (FQuat4f)Transform.GetRotation();
}

static inline void DecomposeTransforms(
	const TArray<FTransform>& Transforms,
	TArray<FVector3f>& OutTranslations,
	TArray<FQuat4f>& OutRotations)
{
	const int32 NumTransforms = Transforms.Num();
	for (int32 Index = 0; Index < NumTransforms; ++Index)
	{
		FVector3f Translation;
		FQuat4f Rotation;
		DecomposeTransform(Transforms[Index], Translation, Rotation);
		OutTranslations.Emplace(MoveTemp(Translation));
		OutRotations.Emplace(MoveTemp(Rotation));
	}
}

static inline uint32 BankFlagsFromSequence(const FAnimBankSequence& BankSequence)
{
	uint32 Flags = ANIM_BANK_FLAG_NONE;

	if (BankSequence.bLooping)
	{
		Flags |= ANIM_BANK_FLAG_LOOPING;
	}

	if (BankSequence.bAutoStart)
	{
		Flags |= ANIM_BANK_FLAG_AUTOSTART;
	}

	return Flags;
}

class FAnimBankAsyncBuildWorker : public FNonAbandonableTask
{
	FAnimBankBuildAsyncCacheTask* Owner;
	FIoHash IoHash;

public:
	FAnimBankAsyncBuildWorker(FAnimBankBuildAsyncCacheTask* InOwner, const FIoHash& InIoHash)
	: Owner(InOwner)
	, IoHash(InIoHash)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAnimBankAsyncBuildWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork();
};

struct FAnimBankAsyncBuildTask : public FAsyncTask<FAnimBankAsyncBuildWorker>
{
	FAnimBankAsyncBuildTask(FAnimBankBuildAsyncCacheTask* InOwner, const FIoHash& InIoHash)
	: FAsyncTask<FAnimBankAsyncBuildWorker>(InOwner, InIoHash)
	{
	}
};

class FAnimBankBuildAsyncCacheTask
{
public:
	FAnimBankBuildAsyncCacheTask(
		const FIoHash& InKeyHash,
		FAnimBankData* InData,
		UAnimBank& InDisplacedMesh,
		const ITargetPlatform* TargetPlatform
	);

	inline void Wait()
	{
		if (bIsWaitingOnCompilation)
		{
			WaitForDependenciesAndBeginCache();
		}

		if (BuildTask != nullptr)
		{
			BuildTask->EnsureCompletion();
		}

		Owner.Wait();
	}

	inline bool WaitWithTimeout(float TimeLimitSeconds)
	{
		if (bIsWaitingOnCompilation)
		{
			if (!WaitForDependenciesAndBeginCacheWithTimeout(TimeLimitSeconds))
			{
				return false;
			}
		}

		if (BuildTask != nullptr && !BuildTask->WaitCompletionWithTimeout(TimeLimitSeconds))
		{
			return false;
		}

		return Owner.Poll();
	}

	inline bool Poll()
	{
		if (bIsWaitingOnCompilation)
		{
			BeginCacheIfDependenciesAreFree();
			return false;
		}

		if (BuildTask && !BuildTask->IsDone())
		{
			return false;
		}

		return Owner.Poll();
	}

	inline void Cancel()
	{
		// Cancel the waiting on the build
		bIsWaitingOnCompilation = false;

		if (BuildTask)
		{
			BuildTask->Cancel();
		}

		Owner.Cancel();
	}

	void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority);

private:
	bool ShouldWaitForCompilation() const;

	void BeginCacheIfDependenciesAreFree();
	void WaitForDependenciesAndBeginCache();
	bool WaitForDependenciesAndBeginCacheWithTimeout(float TimeLimitSeconds);

	void BeginCache(const FIoHash& KeyHash);
	void EndCache(UE::DerivedData::FCacheGetValueResponse&& Response);
	bool BuildData(const UE::FSharedString& Name, const UE::DerivedData::FCacheKey& Key);

private:
	friend class FAnimBankAsyncBuildWorker;
	TUniquePtr<FAnimBankAsyncBuildTask> BuildTask;
	FAnimBankData* Data;
	TWeakObjectPtr<UAnimBank> WeakAnimBank;

	TArray<FAnimBankSequence> BankItems;

	const ITargetPlatform* TargetPlatform = nullptr;

	UE::DerivedData::FRequestOwner Owner;
	TRefCountPtr<IExecutionResource> ExecutionResource;

	bool bIsWaitingOnCompilation;
	FIoHash KeyHash;
};

void FAnimBankAsyncBuildWorker::DoWork()
{
	using namespace UE::DerivedData;
	if (UAnimBank* Bank = Owner->WeakAnimBank.Get())
	{
		// Grab any execution resources currently assigned to this worker so that we maintain
		// concurrency limit and memory pressure until the whole multi-step task is done.
		Owner->ExecutionResource = FExecutionResourceContext::Get();

		static const FCacheBucket Bucket("AnimBank");
		GetCache().GetValue({ {{Bank->GetPathName()}, {Bucket, IoHash}} }, Owner->Owner,
			[Task = Owner](FCacheGetValueResponse&& Response)
			{
				Task->EndCache(MoveTemp(Response));
			}
		);
	}
}

FAnimBankBuildAsyncCacheTask::FAnimBankBuildAsyncCacheTask(
	const FIoHash& InKeyHash,
	FAnimBankData* InData,
	UAnimBank& InBank,
	const ITargetPlatform* InTargetPlatform
)
: Data(InData)
, WeakAnimBank(&InBank)
, BankItems(InBank.Sequences)
, TargetPlatform(InTargetPlatform)
// Once we pass the BeginCache throttling gate, we want to finish as fast as possible
// to avoid holding on to memory for a long time. We use the high priority since it will go fast,
// but also it will avoid starving the critical threads in the subsequent task.
, Owner(UE::DerivedData::EPriority::High)
, bIsWaitingOnCompilation(ShouldWaitForCompilation())
, KeyHash(InKeyHash)
{
	/**
	 * Unfortunately our async builds are not made to handle the assets that use data from other assets
	 * This will delay the start of the actual cache until the build of the sequences is done
	 * This will fix a race condition with the sequence build without blocking the game thread by default.
	 * Note: This is not a perfect solution since it also delays the DDC data pull.
	 */
	if (!bIsWaitingOnCompilation)
	{
		BeginCache(InKeyHash);
	}
}

bool FAnimBankBuildAsyncCacheTask::ShouldWaitForCompilation() const 
{
	if (UAnimBank* Bank = WeakAnimBank.Get())
	{
		for (FAnimBankSequence& BankSequence : Bank->Sequences)
		{
			if (!IsValid(BankSequence.Sequence))
			{
				continue;
			}

			// If the sequence is still waiting for a post load call, let it build its stuff first to avoid blocking the Game Thread
			if (BankSequence.Sequence->HasAnyFlags(RF_NeedPostLoad) || BankSequence.Sequence->IsCompiling())
			{
				return true;
			}
		}
	}

	return false;
}

void FAnimBankBuildAsyncCacheTask::BeginCacheIfDependenciesAreFree()
{
	if (UAnimBank* Bank = WeakAnimBank.Get())
	{
		if (!ShouldWaitForCompilation())
		{
			bIsWaitingOnCompilation = false;
			BeginCache(KeyHash);
		}
	}
	else
	{
		bIsWaitingOnCompilation = false;
	}
}

void FAnimBankBuildAsyncCacheTask::WaitForDependenciesAndBeginCache()
{
	if (UAnimBank* Bank = WeakAnimBank.Get())
	{
		for (FAnimBankSequence& BankSequence : Bank->Sequences)
		{
			if (!BankSequence.Sequence)
			{
				continue;
			}

			if (BankSequence.Sequence->HasAnyFlags(RF_NeedPostLoad))
			{
				BankSequence.Sequence->ConditionalPostLoad();
			}

			UE::Anim::FAnimSequenceCompilingManager::Get().FinishCompilation({ BankSequence.Sequence.Get() });
		}

		bIsWaitingOnCompilation = false;
		BeginCache(KeyHash);
	}
	else
	{
		bIsWaitingOnCompilation = false;
	}
}

bool FAnimBankBuildAsyncCacheTask::WaitForDependenciesAndBeginCacheWithTimeout(float TimeLimitSeconds)
{
	if (UAnimBank* Bank = WeakAnimBank.Get())
	{
		for (FAnimBankSequence& BankSequence : Bank->Sequences)
		{
			if (!BankSequence.Sequence || !BankSequence.Sequence->IsCompiling())
			{
				continue;
			}

			if (!BankSequence.Sequence->WaitForAsyncTasks(TimeLimitSeconds))
			{
				return false;
			}
		}
	}

	// Performs any necessary cleanup now that the async task (if any) is complete
	WaitForDependenciesAndBeginCache();

	return true;
}

void FAnimBankBuildAsyncCacheTask::BeginCache(const FIoHash& InKeyHash)
{
	using namespace UE::DerivedData;

	if (UAnimBank* Bank = WeakAnimBank.Get())
	{
		// Queue this launch through the thread pool so that we benefit from fair scheduling and memory throttling
		FQueuedThreadPool* ThreadPool = FAnimBankCompilingManager::Get().GetThreadPool();
		EQueuedWorkPriority BasePriority = FAnimBankCompilingManager::Get().GetBasePriority(Bank);

		int64 RequiredMemory = -1; // @todo RequiredMemory - possibly follow FAnimationSequenceAsyncCacheTask::GetRequiredMemoryEstimate()

		check(BuildTask == nullptr);
		BuildTask = MakeUnique<FAnimBankAsyncBuildTask>(this, InKeyHash);
		BuildTask->StartBackgroundTask(ThreadPool, BasePriority, EQueuedWorkFlags::DoNotRunInsideBusyWait, RequiredMemory, TEXT("AnimBank"));
	}
}

void FAnimBankBuildAsyncCacheTask::EndCache(UE::DerivedData::FCacheGetValueResponse&& Response)
{
	using namespace UE::DerivedData;

	if (Response.Status == EStatus::Ok)
	{
		Owner.LaunchTask(TEXT("AnimBankSerialize"), [this, Value = MoveTemp(Response.Value)]
		{
			// Release execution resource as soon as the task is done
			ON_SCOPE_EXIT { ExecutionResource = nullptr; };

			if (UAnimBank* Bank = WeakAnimBank.Get())
			{
				FSharedBuffer RecordData = Value.GetData().Decompress();
				FMemoryReaderView Ar(RecordData, /*bIsPersistent*/ true);
				Ar << *Data;

				// The initialization of the resources is done by FAnimBankCompilingManager to avoid race conditions
			}
		});
	}
	else if (Response.Status == EStatus::Error)
	{
		Owner.LaunchTask(TEXT("AnimBankBuild"), [this, Name = Response.Name, Key = Response.Key]
		{
			// Release execution resource as soon as the task is done
			ON_SCOPE_EXIT { ExecutionResource = nullptr; };

			if (!BuildData(Name, Key))
			{
				return;
			}

			if (UAnimBank* Bank = WeakAnimBank.Get())
			{
				TArray64<uint8> RecordData;
				FMemoryWriter64 Ar(RecordData, /*bIsPersistent*/ true);
				Ar << *Data;

				GetCache().PutValue({ {Name, Key, FValue::Compress(MakeSharedBufferFromArray(MoveTemp(RecordData)))} }, Owner);

				// The initialization of the resources is done by FAnimBankCompilingManager to avoid race conditions
			}
		});
	}
	else
	{
		// Release execution resource as soon as the task is done
		ExecutionResource = nullptr;
	}
}

static void GetRefBoneGlobalSpace(const FReferenceSkeleton& RefSkeleton, TArray<FTransform>& Transforms)
{
	Transforms.Reset();

	const TArray<FTransform>& BoneSpaceTransforms = RefSkeleton.GetRawRefBonePose(); // Get only raw bones (no virtual)

	const int32 NumTransforms = BoneSpaceTransforms.Num();
	Transforms.SetNumUninitialized(NumTransforms);

	for (int32 BoneIndex = 0; BoneIndex < NumTransforms; ++BoneIndex)
	{
		// Initialize to identity since some of them don't have tracks
		int32 IterBoneIndex = BoneIndex;
		FTransform ComponentSpaceTransform = BoneSpaceTransforms[BoneIndex];

		do
		{
			const int32 ParentIndex = RefSkeleton.GetRawParentIndex(IterBoneIndex); // Get only raw bones (no virtual)
			if (ParentIndex != INDEX_NONE)
			{
				ComponentSpaceTransform *= BoneSpaceTransforms[ParentIndex];
			}

			IterBoneIndex = ParentIndex;
		}
		while (RefSkeleton.IsValidIndex(IterBoneIndex));

		Transforms[BoneIndex] = ComponentSpaceTransform;
	}
}

bool FAnimBankBuildAsyncCacheTask::BuildData(const UE::FSharedString& Name, const UE::DerivedData::FCacheKey& Key)
{
	using namespace UE::DerivedData;

	UAnimBank* AnimBank = WeakAnimBank.Get();
	if (!AnimBank)
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FAnimBankBuildAsyncCacheTask::BuildData);

	*Data = {};

	if (!IsValid(AnimBank->Asset))
	{
		UE_LOG(LogAnimBank, Error, TEXT("Cannot find a valid skinned asset to build the animation bank asset."));
		return false;
	}

	UE::AnimBank::BuildSkinnedAssetMapping(*AnimBank->Asset.Get(), Data->Mapping);

	const FBoxSphereBounds AssetBounds = AnimBank->Asset->GetBounds();

	const FReferenceSkeleton& MeshRefSkeleton = AnimBank->Asset->GetRefSkeleton();
	const FReferenceSkeleton& AnimRefSkeleton = AnimBank->Asset->GetSkeleton()->GetReferenceSkeleton();

	// Get the skeleton reference pose in local space
	const TArray<FTransform>& MeshLocalRefPose = MeshRefSkeleton.GetRawRefBonePose();
	const TArray<FTransform>& AnimLocalRefPose = AnimRefSkeleton.GetRawRefBonePose();

#if 0
	// Get the skeleton reference pose in global space (needed for delta bounds)
	TArray<FTransform> RefBoneGlobalSpace;
	GetRefBoneGlobalSpace(AnimRefSkeleton, RefBoneGlobalSpace);
#endif

	const int32 NumMeshBones = MeshRefSkeleton.GetRawBoneNum();
	const int32 NumAnimBones = AnimRefSkeleton.GetRawBoneNum();

	// Scratch memory
	BoneTrackArray TrackToBoneIndexMap;

	Data->Entries.SetNum(AnimBank->Sequences.Num());
	for (int32 ItemIndex = 0; ItemIndex < AnimBank->Sequences.Num(); ++ItemIndex)
	{
		if (Owner.IsCanceled())
		{
			return false;
		}

		FAnimBankEntry& BankEntry = Data->Entries[ItemIndex];

		FAnimBankSequence& BankSequence = AnimBank->Sequences[ItemIndex];
		UAnimSequence* Sequence = BankSequence.Sequence.Get();
		if (Sequence == nullptr)
		{
			continue;
		}

		Sequence->FinishAsyncTasks();

		UAnimSequence::FScopedCompressedAnimSequence CompressedAnimSequence = Sequence->GetCompressedData(TargetPlatform);
		const FCompressedAnimSequence& PlatformCompressedData = CompressedAnimSequence.Get();
		// We should always have compressed data at this point
		if (PlatformCompressedData.IsBoneDataValid(Sequence))
		{
			UE_LOG(LogAnimBank, Error, TEXT("Animation bank referenced sequence is missing compressed data!"));
			return false;
		}

		// Set up mapping tables for the decompressor to map internal tracks to the pose array (which is in bone order).
		const int32 NumTracks = PlatformCompressedData.CompressedTrackToSkeletonMapTable.Num();

		TrackToBoneIndexMap.Reset(NumTracks);
		for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
		{
			const int32 BoneIndex = PlatformCompressedData.GetSkeletonIndexFromTrackIndex(TrackIndex);

			// We only care about raw bones.
			if (BoneIndex < NumAnimBones)
			{
				TrackToBoneIndexMap.Add(BoneTrackPair(BoneIndex, TrackIndex));
			}
		}

		// Prep to uncompress a non-additive animation.
		FAnimSequenceDecompressionContext DecompressionContext(
			Sequence->GetSamplingFrameRate(),
			Sequence->GetSamplingFrameRate().AsFrameTime(Sequence->GetPlayLength()).RoundToFrame().Value,
			EAnimInterpolationType::Linear,
			Sequence->GetRetargetTransformsSourceName(),
			*PlatformCompressedData.CompressedDataStructure,
			AnimRefSkeleton.GetRefBonePose(),
			PlatformCompressedData.CompressedTrackToSkeletonMapTable,
			nullptr,
			false,
			AAT_None
		);

		BankEntry.FrameCount = Sequence->GetSamplingFrameRate().AsFrameTime(Sequence->GetPlayLength()).RoundToFrame().Value;
		check(BankEntry.FrameCount > 0);

		BankEntry.KeyCount = BankEntry.FrameCount * NumMeshBones;//  Sequence->GetNumberOfSampledKeys();
		check(BankEntry.KeyCount > 0);

		BankEntry.Flags = BankFlagsFromSequence(BankSequence);
		BankEntry.Position = BankSequence.Position;
		BankEntry.PlayRate = BankSequence.PlayRate;

		// The sampled pose transforms from the sequence are in bone (parent) space. We need to convert them
		// to local space (component space) to retarget to the mesh and create ref -> anim pose transform.  
		TArray<FTransform> SampledLocalAnimPose = AnimRefSkeleton.GetRawRefBonePose();
		TArray<FTransform> SampledLocalMeshPose;
		TArray<FTransform> SampledGlobalMeshPose;

		SampledLocalMeshPose.SetNumUninitialized(NumMeshBones);
		SampledGlobalMeshPose.SetNumUninitialized(NumMeshBones);

		BankEntry.PositionKeys.SetNumUninitialized(BankEntry.KeyCount);
		BankEntry.RotationKeys.SetNumUninitialized(BankEntry.KeyCount);

		// Initialize bounds to the mesh vertex positions in reference pose.
		FVector3f AnimatedBoundsMin(AssetBounds.Origin - AssetBounds.BoxExtent);
		FVector3f AnimatedBoundsMax(AssetBounds.Origin + AssetBounds.BoxExtent);

		VectorRegister4Float VecAnimatedBoundsMin = VectorLoadFloat3(&AnimatedBoundsMin.X);
		VectorRegister4Float VecAnimatedBoundsMax = VectorLoadFloat3(&AnimatedBoundsMax.X);

		int32 KeyIndex = 0;

		for (uint32 Frame = 0; Frame < BankEntry.FrameCount; ++Frame)
		{
			// Some paths in the decompression code use mem stack, so make sure we put a mark here.
			FMemMark Mark(FMemStack::Get());

			const double SeekTime = Sequence->GetSamplingFrameRate().AsSeconds(FFrameTime(int32(Frame)));
			DecompressionContext.Seek(SeekTime);

			TArrayView<FTransform> SampledLocalPoseView(SampledLocalAnimPose);
			PlatformCompressedData.BoneCompressionCodec->DecompressPose(DecompressionContext, TrackToBoneIndexMap, TrackToBoneIndexMap, TrackToBoneIndexMap, SampledLocalPoseView);

			// Retarget from the anim skeleton to the mesh skeleton.
			for (int32 MeshBoneIndex = 0; MeshBoneIndex < NumMeshBones; ++MeshBoneIndex)
			{
				const int32 AnimBoneIndex = Data->Mapping.MeshToAnimIndexMap[MeshBoneIndex];
				if (AnimBoneIndex != INDEX_NONE)
				{
					const TTuple<FQuat, FQuat>& RetargetingItem = Data->Mapping.RetargetingTable[MeshBoneIndex];
					FTransform SourceToTargetTransform(
						RetargetingItem.Get<0>() * AnimLocalRefPose[AnimBoneIndex].GetRotation() * RetargetingItem.Get<1>(),
						RetargetingItem.Get<0>().RotateVector(AnimLocalRefPose[AnimBoneIndex].GetTranslation()),
						FVector::OneVector
					);
			
					SampledLocalMeshPose[MeshBoneIndex].SetRotation(SampledLocalAnimPose[AnimBoneIndex].GetRotation() * SourceToTargetTransform.GetRotation().Inverse() * MeshLocalRefPose[MeshBoneIndex].GetRotation());
					SampledLocalMeshPose[MeshBoneIndex].SetTranslation(SampledLocalAnimPose[AnimBoneIndex].GetTranslation() + (MeshLocalRefPose[MeshBoneIndex].GetTranslation() - SourceToTargetTransform.GetTranslation()));
					SampledLocalMeshPose[MeshBoneIndex].SetScale3D(FVector::OneVector);
				}
				else
				{
					SampledLocalMeshPose[MeshBoneIndex] = FTransform::Identity;
				}
			}
		
			// Convert local pose from the sequence to global pose (in the animation skeleton's space -- we retarget below to the mesh skeleton's space, as needed).
			UE::AnimBank::ConvertLocalToGlobalSpaceTransforms(MeshRefSkeleton, SampledLocalMeshPose, SampledGlobalMeshPose);

			for (int32 MeshBoneIndex = 0; MeshBoneIndex < NumMeshBones; ++MeshBoneIndex)
			{
				BankEntry.PositionKeys[KeyIndex] = FVector3f(SampledGlobalMeshPose[MeshBoneIndex].GetTranslation());
				BankEntry.RotationKeys[KeyIndex] = FQuat4f(SampledGlobalMeshPose[MeshBoneIndex].GetRotation());

				// Expand animated bounds
				{
					const VectorRegister4Float VecBonePosition = VectorLoadFloat3(&BankEntry.PositionKeys[KeyIndex].X);
					VecAnimatedBoundsMin = VectorMin(VecAnimatedBoundsMin, VecBonePosition);
					VecAnimatedBoundsMax = VectorMax(VecAnimatedBoundsMax, VecBonePosition);
				}

				++KeyIndex;
			}
		}

		VectorStoreFloat3(VecAnimatedBoundsMin, &AnimatedBoundsMin);
		VectorStoreFloat3(VecAnimatedBoundsMax, &AnimatedBoundsMax);

		// Calculate (nearly) conservative bounds across all key frames
		// Also accounts for translated root motion
		BankEntry.SampledBounds = FBoxSphereBounds(
			FBox(
				FVector(AnimatedBoundsMin),
				FVector(AnimatedBoundsMax)
			)
		);

		// Apply per-sequence bounds scale (if specified)
		BankEntry.SampledBounds.BoxExtent *= BankSequence.BoundsScale;
		BankEntry.SampledBounds.SphereRadius *= BankSequence.BoundsScale;

		if (BankEntry.SampledBounds.ContainsNaN())
		{
			UE_LOG(LogAnimBank, Error, TEXT("BankEntry contains NaN in sampled bounds!"));
			return false;
		}

		check(BankEntry.PositionKeys.Num() > 0);
		check(BankEntry.RotationKeys.Num() > 0);
		check(BankEntry.FrameCount > 0);
	}

	if (Owner.IsCanceled())
	{
		return false;
	}

	return true;
}

void FAnimBankBuildAsyncCacheTask::Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority)
{
	if (BuildTask)
	{
		BuildTask->Reschedule(InThreadPool, InPriority);
	}
}

#endif

void UAnimBank::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Animation);
	
	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	Super::Serialize(Ar);

	if (Ar.IsFilterEditorOnly() && !Ar.IsObjectReferenceCollector() && !Ar.ShouldSkipBulkData())
	{
	#if WITH_EDITOR
		if (Ar.IsCooking())
		{
			if (IsCompiling())
			{
				FAnimBankCompilingManager::Get().FinishCompilation({this});
			}

			FAnimBankData& CookedData = CacheDerivedData(Ar.CookingTarget());
			Ar << CookedData;
		}
		else
	#endif
		{
			Ar << Data;
		}
	}
}

void UAnimBank::PostLoad()
{
	Super::PostLoad();

	if (FApp::CanEverRender())
	{
		// Only valid for cooked builds
		if (Data.Entries.Num() > 0)
		{
			InitResources();
		}
	#if WITH_EDITOR
		else if (ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform())
		{
			BeginCacheDerivedData(RunningPlatform);
		}
	#endif
	}

#if WITH_EDITOR
	OnDependenciesChanged.Broadcast(this);
#endif
}

void UAnimBank::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);
}

void UAnimBank::BeginDestroy()
{
	ReleaseResources();

#if WITH_EDITOR
	OnDependenciesChanged.Broadcast(this);
#endif

	Super::BeginDestroy();
}

bool UAnimBank::IsReadyForFinishDestroy()
{
	if (!Super::IsReadyForFinishDestroy())
	{
		return false;
	}

#if WITH_EDITOR
	if (!TryCancelAsyncTasks())
	{
		return false;
	}
#endif

	return ReleaseResourcesFence.IsFenceComplete();
}

bool UAnimBank::NeedsLoadForTargetPlatform(const ITargetPlatform* TargetPlatform) const
{
	return DoesTargetPlatformSupportNanite(TargetPlatform);
}

void UAnimBank::InitResources()
{
	if (!FApp::CanEverRender())
	{
		return;
	}

	if (!bIsInitialized)
	{
		// TODO:
		//Data.ResourcesPtr->InitResources(this);
	}

	bIsInitialized = true;
}

void UAnimBank::ReleaseResources()
{
	if (!bIsInitialized)
	{
		return;
	}

	// TODO:
	//if (Data.ResourcesPtr->ReleaseResources())
	{
		// Make sure the renderer is done processing the command,
		// and done using the GPU resources before we overwrite the data.
		ReleaseResourcesFence.BeginFence();
	}

	bIsInitialized = false;
}

#if WITH_EDITOR

void UAnimBank::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!IsTemplate() && !FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		MarkPackageDirty();
	}

	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimBank, Sequences))
		{
			// ...
		}
	}

	OnDependenciesChanged.Broadcast(this);

	// Synchronously build the new data. This calls InitResources.
	ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	BeginCacheDerivedData(RunningPlatform);
}

void UAnimBank::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);
	BeginCacheDerivedData(TargetPlatform);
}

bool UAnimBank::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	const FIoHash KeyHash = CreateDerivedDataKeyHash(TargetPlatform);
	if (KeyHash.IsZero())
	{
		return true;
	}

	if (PollCacheDerivedData(KeyHash))
	{
		EndCacheDerivedData(KeyHash);
		return true;
	}

	return false;
}

void UAnimBank::ClearAllCachedCookedPlatformData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAnimBank::ClearAllCachedCookedPlatformData);

	if (!TryCancelAsyncTasks())
	{
		FinishAsyncTasks();
	}

	/**
	 * TryCancelAsyncTasks or FinishAsyncTasks should have been able to clear all tasks.If any tasks remain
	 * then they must still be running, and we would crash when attempting to delete them.
	*/
	check(CacheTasksByKeyHash.IsEmpty());

	DataByPlatformKeyHash.Empty();
	Super::ClearAllCachedCookedPlatformData();
}

bool UAnimBank::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive)
{
	UAnimationAsset::GetAllAnimationSequencesReferred(AnimationAssets, bRecursive);

	for (FAnimBankSequence& BankSequence : Sequences)
	{
		UAnimSequence* Sequence = BankSequence.Sequence;
		if (Sequence && !AnimationAssets.Contains(Sequence))
		{
			Sequence->HandleAnimReferenceCollection(AnimationAssets, bRecursive);
		}
	}

	return AnimationAssets.Num() > 0;
}

template<class AssetType>
inline void HandleAnimBankReferenceReplacement(AssetType*& OriginalAsset, const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap)
{
	AssetType* CacheOriginalAsset = OriginalAsset;
	OriginalAsset = nullptr;

	if (UAnimationAsset* const* ReplacementAsset = ReplacementMap.Find(CacheOriginalAsset))
	{
		OriginalAsset = Cast<AssetType>(*ReplacementAsset);
	}
}

template<class AssetType>
inline void HandleAnimBankReferenceReplacement(TObjectPtr<AssetType>& OriginalAsset, const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap)
{
	HandleAnimBankReferenceReplacement(static_cast<AssetType*&>(OriginalAsset), ReplacementMap);
}

void UAnimBank::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap)
{
	UAnimationAsset::ReplaceReferredAnimations(ReplacementMap);

	for (FAnimBankSequence& BankSequence : Sequences)
	{
		HandleAnimBankReferenceReplacement(BankSequence.Sequence, ReplacementMap);
	}
}

bool UAnimBank::IsCompiling() const
{
	return CacheTasksByKeyHash.Num() > 0;
}

bool UAnimBank::TryCancelAsyncTasks()
{
	bool bHadCachedTaskForRunningPlatform = CacheTasksByKeyHash.Contains(DataKeyHash);
	
	for (auto It = CacheTasksByKeyHash.CreateIterator(); It; ++It)
	{
		if (It->Value->Poll())
		{
			It.RemoveCurrent();
		}
		else
		{
			It->Value->Cancel();

			// Try to see if we can remove the task now that it might have been canceled
			if (It->Value->Poll())
			{
				It.RemoveCurrent();
			}
		}
	}

	if (bHadCachedTaskForRunningPlatform && !CacheTasksByKeyHash.Contains(DataKeyHash))
	{
		// Reset the cached Key for the running platform since we won't have any GPU data
		DataKeyHash = FIoHash();
	}

	return CacheTasksByKeyHash.IsEmpty();
}

bool UAnimBank::IsAsyncTaskComplete() const
{
	for (auto& Pair : CacheTasksByKeyHash)
	{
		if (!Pair.Value->Poll())
		{
			return false;
		}
	}

	return true;
}

bool UAnimBank::WaitForAsyncTasks(float TimeLimitSeconds)
{
	double StartTimeSeconds = FPlatformTime::Seconds();
	for (auto& Pair : CacheTasksByKeyHash)
	{
		// Clamp to 0 as it implies polling
		const float TimeLimit = FMath::Min(0.0f, TimeLimitSeconds - (FPlatformTime::Seconds() - StartTimeSeconds));
		if (!Pair.Value->WaitWithTimeout(TimeLimit))
		{
			return false;
		}
	}

	return true;
}

void UAnimBank::FinishAsyncTasks()
{
	for (auto It = CacheTasksByKeyHash.CreateIterator(); It; ++It)
	{
		It->Value->Wait();
		It.RemoveCurrent();
	}
}

void UAnimBank::Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority)
{
	for (auto& Pair : CacheTasksByKeyHash)
	{
		Pair.Value->Reschedule(InThreadPool, InPriority);
	}
}

FIoHash UAnimBank::CreateDerivedDataKeyHash(const ITargetPlatform* TargetPlatform)
{
	if (!DoesTargetPlatformSupportNanite(TargetPlatform))
	{
		return FIoHash::Zero;
	}

	FMemoryHasherBlake3 Writer;
	FGuid AnimBankVersionGuid(0x51842261, 0xDBAF4D8C, 0xB13ABA3C, 0x50EC8691);
	Writer << AnimBankVersionGuid;

	if (IsValid(Asset))
	{
		FString AssetHash = Asset->BuildDerivedDataKey(TargetPlatform);
		Writer << AssetHash;
	}

	for (int32 SequenceIndex = 0; SequenceIndex < Sequences.Num(); ++SequenceIndex)
	{
		Writer << SequenceIndex;

		FAnimBankSequence& BankSequence = Sequences[SequenceIndex];
		if (IsValid(BankSequence.Sequence))
		{
			// Make sure all our required dependencies are loaded, we need them to compute the KeyHash
			FAnimationUtils::EnsureAnimSequenceLoaded(*BankSequence.Sequence);

			FIoHash SequenceHash = BankSequence.Sequence->GetDerivedDataKeyHash(TargetPlatform);
			Writer << SequenceHash;
		}

		uint32 Flags = BankFlagsFromSequence(BankSequence);
		Writer << Flags;

		Writer << BankSequence.Position;
		Writer << BankSequence.PlayRate;
		Writer << BankSequence.BoundsScale;
	}

#if PLATFORM_CPU_ARM_FAMILY
	// Separate out arm keys as x64 and arm64 clang do not generate the same data for a given
	// input. Add the arm specifically so that a) we avoid rebuilding the current DDC and
	// b) we can remove it once we get arm64 to be consistent.
	FString ArmSuffix(TEXT("_arm64"));
	Writer << ArmSuffix;
#endif

	return Writer.Finalize();
}

FIoHash UAnimBank::BeginCacheDerivedData(const ITargetPlatform* TargetPlatform)
{
	const FIoHash KeyHash = CreateDerivedDataKeyHash(TargetPlatform);

	if (KeyHash.IsZero() || DataKeyHash == KeyHash || DataByPlatformKeyHash.Contains(KeyHash))
	{
		return KeyHash;
	}

	// If nothing has changed and we already started the compilation we should be all good.
	if (CacheTasksByKeyHash.Contains(KeyHash))
	{
		return KeyHash;
	}

	// Make sure we finish the previous build before starting another one
	FAnimBankCompilingManager::Get().FinishCompilation({ this });

	// Make sure the GPU is no longer referencing the current GPU resource data.
	ReleaseResources();
	ReleaseResourcesFence.Wait();
	Data = {};

	NotifyOnGPUDataChanged();

	FAnimBankData* TargetData = nullptr;
	if (TargetPlatform->IsRunningPlatform())
	{
		DataKeyHash = KeyHash;
		TargetData = &Data;
	}
	else
	{
		TargetData = DataByPlatformKeyHash.Emplace(KeyHash, MakeUnique<FAnimBankData>()).Get();
	}

	for (FAnimBankSequence& BankSequence : Sequences)
	{
		UAnimSequence* Sequence = BankSequence.Sequence;
		if (!IsValid(Sequence))
		{
			continue;
		}

		Sequence->BeginCacheDerivedData(TargetPlatform);
	}

	CacheTasksByKeyHash.Emplace(KeyHash, MakePimpl<FAnimBankBuildAsyncCacheTask>(KeyHash, TargetData, *this, TargetPlatform));

	// The compiling manager provides throttling, notification manager, etc... for the asset being built.
	FAnimBankCompilingManager::Get().AddAnimBanks({this});

	return KeyHash;
}

bool UAnimBank::PollCacheDerivedData(const FIoHash& KeyHash) const
{
	if (KeyHash.IsZero())
	{
		return true;
	}

	if (const TPimplPtr<FAnimBankBuildAsyncCacheTask>* Task = CacheTasksByKeyHash.Find(KeyHash))
	{
		return (*Task)->Poll();
	}

	return true;
}

void UAnimBank::EndCacheDerivedData(const FIoHash& KeyHash)
{
	if (KeyHash.IsZero())
	{
		return;
	}

	TPimplPtr<FAnimBankBuildAsyncCacheTask> Task;
	if (CacheTasksByKeyHash.RemoveAndCopyValue(KeyHash, Task))
	{
		Task->Wait();
	}
}

FAnimBankData& UAnimBank::CacheDerivedData(const ITargetPlatform* TargetPlatform)
{
	const FIoHash KeyHash = BeginCacheDerivedData(TargetPlatform);
	EndCacheDerivedData(KeyHash);
	FAnimBankData& AnimBankData = (DataKeyHash == KeyHash) ? Data : *DataByPlatformKeyHash[KeyHash];
	return AnimBankData;
}

FDelegateHandle UAnimBank::RegisterOnGPUDataChanged(const FOnRebuild& Delegate)
{
	return OnGPUDataChanged.Add(Delegate);
}

void UAnimBank::UnregisterOnGPUDataChanged(FDelegateUserObject Unregister)
{
	OnGPUDataChanged.RemoveAll(Unregister);
}

void UAnimBank::UnregisterOnGPUDataChanged(FDelegateHandle Handle)
{
	OnGPUDataChanged.Remove(Handle);
}

void UAnimBank::NotifyOnGPUDataChanged()
{
	OnGPUDataChanged.Broadcast();
}

#endif

void FAnimBankSequence::ValidatePosition()
{
	float Min = 0.0f, Max = 0.0f;

	if (Sequence)
	{
		Max = Sequence->GetPlayLength();
	}

	Position = FMath::Clamp<float>(Position, Min, Max);
}

uint32 FAnimBankDesc::GetHash() const
{
	struct FHashKey
	{
		uint32	BankHash;
		uint32	SequenceIndex;
		uint32	AssetHash;
		float	Position;
		float	PlayRate;
		uint8	Flags;

		static inline uint32 PointerHash(const void* Key)
		{
		#if PLATFORM_64BITS
			// Ignoring the lower 4 bits since they are likely zero anyway.
			// Higher bits are more significant in 64 bit builds.
			return reinterpret_cast<UPTRINT>(Key) >> 4;
		#else
			return reinterpret_cast<UPTRINT>(Key);
		#endif
		};

	} HashKey;

	FMemory::Memzero(HashKey);

	const UAnimBank* BankPtr = BankAsset.Get();
	const USkinnedAsset* AssetPtr = Asset.Get();

	HashKey.BankHash		= FHashKey::PointerHash(BankPtr);
	HashKey.SequenceIndex	= SequenceIndex;
	HashKey.AssetHash		= FHashKey::PointerHash(AssetPtr);
	HashKey.Flags			= 0;
	HashKey.Flags		   |= bLooping   ? 0x1u : 0x0u;
	HashKey.Flags		   |= bAutoStart ? 0x2u : 0x0u;
	HashKey.Position		= Position;
	HashKey.PlayRate		= PlayRate;

	const uint64 DescHash = CityHash64((char*)&HashKey, sizeof(FHashKey));
	return HashCombineFast(uint32(DescHash & 0xFFFFFFFF), uint32((DescHash >> 32) & 0xFFFFFFFF));
}

namespace UE::AnimBank
{

void ConvertLocalToGlobalSpaceTransforms(
	const FReferenceSkeleton& InRefSkeleton,
	const TArray<FTransform>& InLocalSpaceTransforms,
	TArray<FTransform>& OutGlobalSpaceTransforms
)
{
	OutGlobalSpaceTransforms.Reset();

	if (!ensure(InRefSkeleton.GetRawBoneNum() == InLocalSpaceTransforms.Num()))
	{
		return;
	}

	const int32 NumTransforms = InLocalSpaceTransforms.Num();
	OutGlobalSpaceTransforms.SetNumUninitialized(NumTransforms);

	for (int32 BoneIndex = 0; BoneIndex < NumTransforms; ++BoneIndex)
	{
		// Initialize to identity since some of them don't have tracks
		int32 IterBoneIndex = BoneIndex;
		FTransform ComponentSpaceTransform = InLocalSpaceTransforms[BoneIndex];

		do
		{
			const int32 ParentIndex = InRefSkeleton.GetRawParentIndex(IterBoneIndex); // Get only raw bones (no virtual)
			if (ParentIndex != INDEX_NONE)
			{
				ComponentSpaceTransform *= InLocalSpaceTransforms[ParentIndex];
			}

			IterBoneIndex = ParentIndex;
		}
		while (InRefSkeleton.IsValidIndex(IterBoneIndex));

		OutGlobalSpaceTransforms[BoneIndex] = ComponentSpaceTransform;
	}
}

void BuildSkinnedAssetMapping(const USkinnedAsset& Asset, FSkinnedAssetMapping& Mapping)
{
	const FReferenceSkeleton& MeshRefSkeleton = Asset.GetRefSkeleton();
	const FReferenceSkeleton& AnimRefSkeleton = Asset.GetSkeleton()->GetReferenceSkeleton();

	const int32 NumMeshBones = MeshRefSkeleton.GetRawBoneNum();
	const int32 NumAnimBones = AnimRefSkeleton.GetRawBoneNum();

	// Get Number of RawBones (no virtual)
	Mapping.BoneCount = AnimRefSkeleton.GetRawBoneNum();

	// Get the skeleton reference pose in local space;
	const TArray<FTransform>& MeshLocalRefPose = MeshRefSkeleton.GetRawRefBonePose();
	const TArray<FTransform>& AnimLocalRefPose = AnimRefSkeleton.GetRawRefBonePose();

	// Get the bone transforms in global pose.
	ConvertLocalToGlobalSpaceTransforms(MeshRefSkeleton, MeshRefSkeleton.GetRawRefBonePose(), Mapping.MeshGlobalRefPose);
	ConvertLocalToGlobalSpaceTransforms(AnimRefSkeleton, AnimRefSkeleton.GetRawRefBonePose(), Mapping.AnimGlobalRefPose);

	// A map to go from the mesh skeleton bone index to anim skeleton bone index.
	Mapping.MeshToAnimIndexMap.SetNumUninitialized(NumMeshBones);
	for (int32 MeshBoneIndex = 0; MeshBoneIndex < NumMeshBones; ++MeshBoneIndex)
	{
		Mapping.MeshToAnimIndexMap[MeshBoneIndex] = AnimRefSkeleton.FindRawBoneIndex(MeshRefSkeleton.GetBoneName(MeshBoneIndex));
	}

	// Construct a retargeting table to go from the anim skeleton to the mesh skeleton.
	Mapping.RetargetingTable.SetNumUninitialized(NumMeshBones);
	Mapping.RetargetingTable[0] = MakeTuple(FQuat::Identity, AnimRefSkeleton.GetRawRefBonePose()[0].GetRotation().Inverse() * MeshRefSkeleton.GetRawRefBonePose()[0].GetRotation());
	for (int32 MeshBoneIndex = 1; MeshBoneIndex < NumMeshBones; ++MeshBoneIndex)
	{
		const int32 AnimBoneIndex = Mapping.MeshToAnimIndexMap[MeshBoneIndex];
		if (AnimBoneIndex != INDEX_NONE)
		{
			const int32 AnimParentIndex = AnimRefSkeleton.GetParentIndex(AnimBoneIndex);
			const int32 MeshParentIndex = MeshRefSkeleton.GetParentIndex(MeshBoneIndex);
			check(AnimParentIndex != INDEX_NONE);
			check(MeshParentIndex != INDEX_NONE);

			const FQuat Pa = Mapping.AnimGlobalRefPose[AnimParentIndex].GetRotation();
			const FQuat Pm = Mapping.MeshGlobalRefPose[MeshParentIndex].GetRotation();

			const FQuat Ra = AnimLocalRefPose[AnimBoneIndex].GetRotation();
			const FQuat Rm = MeshLocalRefPose[MeshBoneIndex].GetRotation();

			const FQuat Q0 = Pm.Inverse() * Pa;
			const FQuat Q1 = Ra.Inverse() * Pa.Inverse() * Pm * Rm;

			Mapping.RetargetingTable[MeshBoneIndex] = MakeTuple(Q0, Q1);
		}
		else
		{
			Mapping.RetargetingTable[MeshBoneIndex] = MakeTuple(FQuat::Identity, FQuat::Identity);
		}
	}

	Mapping.PositionKeys.AddUninitialized(NumMeshBones);
	Mapping.RotationKeys.AddUninitialized(NumMeshBones);
	for (int32 MeshBoneIndex = 0; MeshBoneIndex < NumMeshBones; ++MeshBoneIndex)
	{
		const FTransform InvMeshGlobalRefPoseXform = Mapping.MeshGlobalRefPose[MeshBoneIndex].Inverse();
		Mapping.PositionKeys[MeshBoneIndex] = FVector3f(InvMeshGlobalRefPoseXform.GetTranslation());
		Mapping.RotationKeys[MeshBoneIndex] = FQuat4f(InvMeshGlobalRefPoseXform.GetRotation());
	}
}

} // UE::AnimBank

FAnimBankItem::FAnimBankItem() = default;

FAnimBankItem::FAnimBankItem(const FAnimBankItem& InBankItem)
{
	BankAsset		= InBankItem.BankAsset;
	SequenceIndex	= InBankItem.SequenceIndex;
}

FAnimBankItem::FAnimBankItem(const FSoftAnimBankItem& InBankItem)
{
	BankAsset		= InBankItem.BankAsset.LoadSynchronous();
	SequenceIndex	= InBankItem.SequenceIndex;
}

bool FAnimBankItem::operator!=(const FAnimBankItem& Other) const
{
	return !(*this == Other);
}

bool FAnimBankItem::operator==(const FAnimBankItem& Other) const
{
	return BankAsset == Other.BankAsset && SequenceIndex == Other.SequenceIndex;
}

FSoftAnimBankItem::FSoftAnimBankItem() = default;

FSoftAnimBankItem::FSoftAnimBankItem(const FSoftAnimBankItem& InBankItem)
{
	BankAsset		= InBankItem.BankAsset.Get();
	SequenceIndex	= InBankItem.SequenceIndex;
}

FSoftAnimBankItem::FSoftAnimBankItem(const FAnimBankItem& InBankItem)
{
	BankAsset		= InBankItem.BankAsset.Get();
	SequenceIndex	= InBankItem.SequenceIndex;
}

bool FSoftAnimBankItem::operator!=(const FSoftAnimBankItem& Other) const
{
	return !(*this == Other);
}

bool FSoftAnimBankItem::operator==(const FSoftAnimBankItem& Other) const
{
	return BankAsset == Other.BankAsset && SequenceIndex == Other.SequenceIndex;
}

FSkinnedMeshComponentDescriptorBase::FSkinnedMeshComponentDescriptorBase()
{
	// Note: should not really be used - prefer using FSkinnedMeshComponentDescriptor or FSoftSkinnedMeshComponentDescriptor
	InitFrom(UInstancedSkinnedMeshComponent::StaticClass()->GetDefaultObject<UInstancedSkinnedMeshComponent>());
}

FSkinnedMeshComponentDescriptorBase::FSkinnedMeshComponentDescriptorBase(ENoInit) {}
FSkinnedMeshComponentDescriptorBase::FSkinnedMeshComponentDescriptorBase(const FSkinnedMeshComponentDescriptorBase& InDescriptor) = default;
FSkinnedMeshComponentDescriptorBase::~FSkinnedMeshComponentDescriptorBase() = default;

void FSkinnedMeshComponentDescriptorBase::InitFrom(const UInstancedSkinnedMeshComponent* Template, bool bInitBodyInstance)
{
	check(Template);

	Mobility = Template->Mobility;
	InstanceMinDrawDistance = Template->InstanceMinDrawDistance;
	Template->GetCullDistances(InstanceStartCullDistance, InstanceEndCullDistance);
	ComponentClass = Template->GetClass();
	bCastShadow = Template->CastShadow;
	bCastDynamicShadow = Template->bCastDynamicShadow;
	bCastStaticShadow = Template->bCastStaticShadow;
	bCastVolumetricTranslucentShadow = Template->bCastVolumetricTranslucentShadow;
	bCastContactShadow = Template->bCastContactShadow;
	bSelfShadowOnly = Template->bSelfShadowOnly;
	bCastFarShadow = Template->bCastFarShadow;
	bCastInsetShadow = Template->bCastInsetShadow;
	bCastCinematicShadow = Template->bCastCinematicShadow;
	bCastShadowAsTwoSided = Template->bCastShadowAsTwoSided;
	bVisibleInRayTracing = Template->bVisibleInRayTracing;
	bAffectDynamicIndirectLighting = Template->bAffectDynamicIndirectLighting;
	bAffectDistanceFieldLighting = Template->bAffectDistanceFieldLighting;
	PrimitiveBoundsOverride = Template->GetPrimitiveBoundsOverride();
#if WITH_EDITOR
	HLODBatchingPolicy = Template->HLODBatchingPolicy;
	bIncludeInHLOD = Template->bEnableAutoLODGeneration;
#endif

	bIsInstanceDataGPUOnly = Template->UsesGPUOnlyInstances();
	if (bIsInstanceDataGPUOnly)
	{
		NumInstancesGPUOnly = Template->GetInstanceCountGPUOnly();
		NumCustomDataFloatsGPUOnly = Template->GetNumCustomDataFloats();
	}
}

void FSkinnedMeshComponentDescriptorBase::InitComponent(UInstancedSkinnedMeshComponent* Component) const
{
	Component->Mobility = Mobility;
	Component->InstanceMinDrawDistance = InstanceMinDrawDistance;
	Component->SetCullDistances(InstanceStartCullDistance, InstanceEndCullDistance);
	Component->CastShadow = bCastShadow;
	Component->bCastDynamicShadow = bCastDynamicShadow;
	Component->bCastStaticShadow = bCastStaticShadow;
	Component->bCastVolumetricTranslucentShadow = bCastVolumetricTranslucentShadow;
	Component->bCastContactShadow = bCastContactShadow;
	Component->bSelfShadowOnly = bSelfShadowOnly;
	Component->bCastFarShadow = bCastFarShadow;
	Component->bCastInsetShadow = bCastInsetShadow;
	Component->bCastCinematicShadow = bCastCinematicShadow;
	Component->bCastShadowAsTwoSided = bCastShadowAsTwoSided;
	Component->bVisibleInRayTracing = bVisibleInRayTracing;
	Component->bAffectDynamicIndirectLighting = bAffectDynamicIndirectLighting;
	Component->bAffectDistanceFieldLighting = bAffectDistanceFieldLighting;
	Component->SetPrimitiveBoundsOverride(PrimitiveBoundsOverride);
#if WITH_EDITOR
	Component->HLODBatchingPolicy = HLODBatchingPolicy;
	Component->bEnableAutoLODGeneration = bIncludeInHLOD;
#endif

	Component->SetInstanceDataGPUOnly(bIsInstanceDataGPUOnly);
	if (bIsInstanceDataGPUOnly)
	{
		Component->SetNumGPUInstances(NumInstancesGPUOnly);
		Component->SetNumCustomDataFloats(NumCustomDataFloatsGPUOnly);
	}
}

bool FSkinnedMeshComponentDescriptorBase::operator!=(const FSkinnedMeshComponentDescriptorBase& Other) const
{
	return !(*this == Other);
}

bool FSkinnedMeshComponentDescriptorBase::operator==(const FSkinnedMeshComponentDescriptorBase& Other) const
{
	return
		ComponentClass == Other.ComponentClass &&
		Mobility == Other.Mobility &&
		InstanceMinDrawDistance == Other.InstanceMinDrawDistance &&
		InstanceStartCullDistance == Other.InstanceStartCullDistance &&
		InstanceEndCullDistance == Other.InstanceEndCullDistance &&
#if WITH_EDITOR
		HLODBatchingPolicy == Other.HLODBatchingPolicy &&
		bIncludeInHLOD == Other.bIncludeInHLOD &&
#endif 
		bCastShadow == Other.bCastShadow &&
		bCastDynamicShadow == Other.bCastDynamicShadow &&
		bCastStaticShadow == Other.bCastStaticShadow &&
		bCastVolumetricTranslucentShadow == Other.bCastVolumetricTranslucentShadow &&
		bCastContactShadow == Other.bCastContactShadow &&
		bSelfShadowOnly == Other.bSelfShadowOnly &&
		bCastFarShadow == Other.bCastFarShadow &&
		bCastInsetShadow == Other.bCastInsetShadow &&
		bCastCinematicShadow == Other.bCastCinematicShadow &&
		bCastShadowAsTwoSided == Other.bCastShadowAsTwoSided &&
		bIsInstanceDataGPUOnly == Other.bIsInstanceDataGPUOnly &&
		PrimitiveBoundsOverride == Other.PrimitiveBoundsOverride &&
		NumInstancesGPUOnly == Other.NumInstancesGPUOnly &&
		NumCustomDataFloatsGPUOnly == Other.NumCustomDataFloatsGPUOnly &&
		bVisibleInRayTracing == Other.bVisibleInRayTracing &&
		bAffectDynamicIndirectLighting == Other.bAffectDynamicIndirectLighting &&
		bAffectDistanceFieldLighting == Other.bAffectDistanceFieldLighting;
}

FSkinnedMeshComponentDescriptor::FSkinnedMeshComponentDescriptor()
	: FSkinnedMeshComponentDescriptorBase(NoInit)
{
	// Make sure we have proper defaults
	InitFrom(UInstancedSkinnedMeshComponent::StaticClass()->GetDefaultObject<UInstancedSkinnedMeshComponent>());
}

FSkinnedMeshComponentDescriptor::FSkinnedMeshComponentDescriptor(ENoInit) {}
FSkinnedMeshComponentDescriptor::FSkinnedMeshComponentDescriptor(const FSkinnedMeshComponentDescriptor& InDescriptor) = default;
FSkinnedMeshComponentDescriptor::~FSkinnedMeshComponentDescriptor() = default;

FSkinnedMeshComponentDescriptor::FSkinnedMeshComponentDescriptor(const FSoftSkinnedMeshComponentDescriptor& InDescriptor)
	: FSkinnedMeshComponentDescriptorBase(InDescriptor)
{
	SkinnedAsset = InDescriptor.SkinnedAsset.LoadSynchronous();
	Algo::Transform(InDescriptor.BankItems, BankItems, [](const FSoftAnimBankItem& BankItem) { return FAnimBankItem(BankItem); });
	// AB-TODO: Algo::Transform(InDescriptor.OverrideMaterials, OverrideMaterials, [](TSoftObjectPtr<UMaterialInterface> Material) { return Material.LoadSynchronous(); });
	// AB-TODO: OverlayMaterial = InDescriptor.OverlayMaterial.LoadSynchronous();
	// AB-TODO: Algo::Transform(InDescriptor.RuntimeVirtualTextures, RuntimeVirtualTextures, [](TSoftObjectPtr<URuntimeVirtualTexture> RVT) { return RVT.LoadSynchronous(); });
	Hash = InDescriptor.Hash;
}

UInstancedSkinnedMeshComponent* FSkinnedMeshComponentDescriptor::CreateComponent(UObject* Outer, FName Name, EObjectFlags ObjectFlags) const
{
	UInstancedSkinnedMeshComponent* Component = NewObject<UInstancedSkinnedMeshComponent>(Outer, ComponentClass, Name, ObjectFlags);
	InitComponent(Component);
	return Component;
}

void FSkinnedMeshComponentDescriptor::InitFrom(const UInstancedSkinnedMeshComponent* Template, bool bInitBodyInstance)
{
	Super::InitFrom(Template, bInitBodyInstance);

	SkinnedAsset = Template->GetSkinnedAsset();
	BankItems = Template->GetAnimBankItems();
}

uint32 FSkinnedMeshComponentDescriptor::ComputeHash() const
{
	FSkinnedMeshComponentDescriptor& MutableSelf = *const_cast<FSkinnedMeshComponentDescriptor*>(this);
	Hash = 0; // we don't want the hash to impact the calculation
	
	FArchiveCrc32 CrcArchive;
	CrcArchive << *this;
	Hash = CrcArchive.GetCrc();

	return Hash;
}

void FSkinnedMeshComponentDescriptor::InitComponent(UInstancedSkinnedMeshComponent* Component) const
{
	Super::InitComponent(Component);
	Component->SetSkinnedAsset(SkinnedAsset);
	Component->SetAnimBankItems(BankItems);
}

void FSkinnedMeshComponentDescriptor::PostLoadFixup(UObject* Loader)
{
	check(Loader);
}

bool FSkinnedMeshComponentDescriptor::operator!=(const FSkinnedMeshComponentDescriptor& Other) const
{
	return !(*this == Other);
}

bool FSkinnedMeshComponentDescriptor::operator==(const FSkinnedMeshComponentDescriptor& Other) const
{
	return
		(Hash == 0 || Other.Hash == 0 || Hash == Other.Hash) && // Check hash first, other checks are in case of Hash collision
		SkinnedAsset == Other.SkinnedAsset &&
		BankItems == Other.BankItems &&
		Super::operator==(Other);
}

FSoftSkinnedMeshComponentDescriptor::FSoftSkinnedMeshComponentDescriptor()
	: FSkinnedMeshComponentDescriptorBase(NoInit)
{
	// Make sure we have proper defaults
	InitFrom(UInstancedSkinnedMeshComponent::StaticClass()->GetDefaultObject<UInstancedSkinnedMeshComponent>());
}

FSoftSkinnedMeshComponentDescriptor::FSoftSkinnedMeshComponentDescriptor(ENoInit) {}
FSoftSkinnedMeshComponentDescriptor::FSoftSkinnedMeshComponentDescriptor(const FSoftSkinnedMeshComponentDescriptor& InDescriptor) = default;
FSoftSkinnedMeshComponentDescriptor::~FSoftSkinnedMeshComponentDescriptor() = default;

FSoftSkinnedMeshComponentDescriptor::FSoftSkinnedMeshComponentDescriptor(const FSkinnedMeshComponentDescriptor& InDescriptor)
	: FSkinnedMeshComponentDescriptorBase(InDescriptor)
{
	SkinnedAsset = InDescriptor.SkinnedAsset;
	Algo::Transform(InDescriptor.BankItems, BankItems, [](const FAnimBankItem& BankItem) { return FSoftAnimBankItem(BankItem); });
	// AB-TODO: Algo::Transform(InDescriptor.OverrideMaterials, OverrideMaterials, [](TObjectPtr<UMaterialInterface> Material) { return Material; });
	// AB-TODO: OverlayMaterial = InDescriptor.OverlayMaterial;
	// AB-TODO: Algo::Transform(InDescriptor.RuntimeVirtualTextures, RuntimeVirtualTextures, [](TObjectPtr<URuntimeVirtualTexture> RVT) { return RVT; });
	Hash = InDescriptor.Hash;
}

UInstancedSkinnedMeshComponent* FSoftSkinnedMeshComponentDescriptor::CreateComponent(UObject* Outer, FName Name, EObjectFlags ObjectFlags) const
{
	UInstancedSkinnedMeshComponent* Component = NewObject<UInstancedSkinnedMeshComponent>(Outer, ComponentClass, Name, ObjectFlags);
	InitComponent(Component);
	return Component;
}

void FSoftSkinnedMeshComponentDescriptor::InitFrom(const UInstancedSkinnedMeshComponent* Template, bool bInitBodyInstance)
{
	Super::InitFrom(Template, bInitBodyInstance);
	SkinnedAsset = Template->GetSkinnedAsset();
	BankItems = Template->GetAnimBankItems();
}

uint32 FSoftSkinnedMeshComponentDescriptor::ComputeHash() const
{
	Hash = 0; // we don't want the hash to impact the calculation
	
	FArchiveCrc32 CrcArchive;
	CrcArchive << *this;
	Hash = CrcArchive.GetCrc();

	return Hash;
}

void FSoftSkinnedMeshComponentDescriptor::InitComponent(UInstancedSkinnedMeshComponent* Component) const
{
	Super::InitComponent(Component);

	Component->SetSkinnedAsset(SkinnedAsset.LoadSynchronous());
	TArray<FAnimBankItem> AnimBankItems;
	for (const FSoftAnimBankItem& BankItem : BankItems)
	{
		AnimBankItems.Emplace(BankItem);
	}
	Component->SetAnimBankItems(AnimBankItems);
}

void FSoftSkinnedMeshComponentDescriptor::PostLoadFixup(UObject* Loader)
{
	check(Loader);
}

bool FSoftSkinnedMeshComponentDescriptor::operator!=(const FSoftSkinnedMeshComponentDescriptor& Other) const
{
	return !(*this == Other);
}

bool FSoftSkinnedMeshComponentDescriptor::operator==(const FSoftSkinnedMeshComponentDescriptor& Other) const
{
	return
		(Hash == 0 || Other.Hash == 0 || Hash == Other.Hash) && // Check hash first, other checks are in case of Hash collision
		SkinnedAsset == Other.SkinnedAsset &&
		BankItems == Other.BankItems &&
		Super::operator==(Other);
}

#undef LOCTEXT_NAMESPACE

