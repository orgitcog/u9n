// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeterogeneousVolumes.h"
#include "HeterogeneousVolumeInterface.h"

#include "LightRendering.h"
#include "PixelShaderUtils.h"
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#include "RayTracingInstanceBufferUtil.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PrimitiveDrawingUtils.h"
#include "VolumeLighting.h"
#include "VolumetricFog.h"

class FGenerateRayMarchingTiles : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateRayMarchingTiles);
	SHADER_USE_PARAMETER_STRUCT(FGenerateRayMarchingTiles, FGlobalShader);

	class FDebugDim : SHADER_PERMUTATION_BOOL("DIM_DEBUG");
	class FVoxelCullingDim : SHADER_PERMUTATION_BOOL("DIM_VOXEL_CULLING");
	using FPermutationDomain = TShaderPermutationDomain<FDebugDim, FVoxelCullingDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Scene data
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)

		// Object data
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(FMatrix44f, WorldToLocal)

		// Sparse voxel data
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSparseVoxelUniformBufferParameters, SparseVoxelUniformBuffer)

		// Ray data
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, StepSize)
		SHADER_PARAMETER(int, MaxStepCount)
		SHADER_PARAMETER(int, bJitter)

		// Dispatch data
		SHADER_PARAMETER(FIntVector, GroupCount)
		SHADER_PARAMETER(int32, DownsampleFactor)

		// Debug Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Volumes::FRayMarchingDebug>, RWRayMarchingDebugBuffer)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumRayMarchingTilesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Volumes::FRayMarchingTile>, RWRayMarchingTilesBuffer)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumVoxelsPerTileBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVoxelDataPacked>, RWVoxelsPerTileBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters
	)
	{
		return DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment
	)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());

		// This shader takes a very long time to compile with FXC, so we pre-compile it with DXC first and then forward the optimized HLSL to FXC.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FGenerateRayMarchingTiles, "/Engine/Private/HeterogeneousVolumes/HeterogeneousVolumesPreshadingPipeline.usf", "GenerateRayMarchingTiles", SF_Compute);

namespace HeterogeneousVolumes
{
	struct FRayMarchingTile
	{
		FIntPoint PixelOffset;
		uint32 Voxels[2];

		uint32 Id;
		uint32 Padding[3];
	};

	struct FRayMarchingDebug
	{
		FVector4f Planes[5];
		FVector4f BBox[2];

		float Padding[4];
	};
}

void GenerateRayMarchingTiles(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	// Sparse voxel data
	FRDGBufferRef NumVoxelsBuffer,
	const TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters>& SparseVoxelUniformBuffer,
	// Output
	FRDGBufferRef& NumRayMarchingTilesBuffer,
	FRDGBufferRef& RayMarchingTilesBuffer,
	FRDGBufferRef& VoxelsPerTileBuffer
)
{
	FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(HeterogeneousVolumes::GetScaledViewRect(View.ViewRect), FGenerateRayMarchingTiles::GetThreadGroupSize2D());
	uint32 NumTiles = GroupCount.X * GroupCount.Y;

	NumRayMarchingTilesBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1),
		TEXT("HeterogeneousVolume.NumRayMarchingTilesBuffer")
	);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(NumRayMarchingTilesBuffer, PF_R32_UINT), 0);

	RayMarchingTilesBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(HeterogeneousVolumes::FRayMarchingTile), NumTiles),
		TEXT("HeterogeneousVolumes.RayMarchingTileBuffer")
	);

	FRDGBufferRef RayMarchingDebugBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(HeterogeneousVolumes::FRayMarchingDebug), NumTiles),
		TEXT("HeterogeneousVolume.RayMarchingDebugBuffer")
	);

	FRDGBufferRef NumVoxelsPerTileBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumTiles),
		TEXT("HeterogeneousVolume.NumVoxelsPerTileBuffer")
	);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(NumVoxelsPerTileBuffer, PF_R32_UINT), 0);

	FIntVector VolumeResolution = SparseVoxelUniformBuffer->GetParameters()->VolumeResolution;
	uint32 SparseMipLevel = SparseVoxelUniformBuffer->GetParameters()->MipLevel;
	FIntVector SparseVolumeResolution = FIntVector(VolumeResolution.X >> SparseMipLevel,
		VolumeResolution.Y >> SparseMipLevel,
		VolumeResolution.Z >> SparseMipLevel
	);

	// TODO: Tight frustum culling guarantees no more than Length(SparseVolumeResolution) but approximate intersection cannot guarantee even L1 distance..
	//uint32 DiagonalLength = FMath::CeilToInt(FMath::Sqrt(SparseVolumeResolution.X * SparseVolumeResolution.Y * SparseVolumeResolution.Z));
	uint32 DiagonalLength = SparseVolumeResolution.X * SparseVolumeResolution.Y * SparseVolumeResolution.Z;

	VoxelsPerTileBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FVoxelDataPacked), NumTiles * DiagonalLength),
		TEXT("HeterogeneousVolumes.VoxelsPerTileBuffer")
	);

	FGenerateRayMarchingTiles::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateRayMarchingTiles::FParameters>();
	{
		// Scene data
		PassParameters->View = View.ViewUniformBuffer;

		// Sparse voxel data
		PassParameters->SparseVoxelUniformBuffer = SparseVoxelUniformBuffer;

		// Ray data
		PassParameters->MaxTraceDistance = HeterogeneousVolumes::GetMaxTraceDistance();
		PassParameters->StepSize = HeterogeneousVolumes::GetStepSize();
		PassParameters->MaxStepCount = HeterogeneousVolumes::GetMaxStepCount();
		PassParameters->bJitter = HeterogeneousVolumes::ShouldJitter();

		// Dispatch data
		PassParameters->GroupCount = GroupCount;
		PassParameters->DownsampleFactor = HeterogeneousVolumes::GetDownsampleFactor();

		// Debug
		PassParameters->RWRayMarchingDebugBuffer = GraphBuilder.CreateUAV(RayMarchingDebugBuffer);

		// Output
		PassParameters->RWNumRayMarchingTilesBuffer = GraphBuilder.CreateUAV(NumRayMarchingTilesBuffer, PF_R32_UINT);
		PassParameters->RWRayMarchingTilesBuffer = GraphBuilder.CreateUAV(RayMarchingTilesBuffer);

		PassParameters->RWNumVoxelsPerTileBuffer = GraphBuilder.CreateUAV(NumVoxelsPerTileBuffer, PF_R32_UINT);
		PassParameters->RWVoxelsPerTileBuffer = GraphBuilder.CreateUAV(VoxelsPerTileBuffer);
	}

	FGenerateRayMarchingTiles::FPermutationDomain PermutationVector;
	PermutationVector.Set<FGenerateRayMarchingTiles::FDebugDim>(HeterogeneousVolumes::GetDebugMode() != 0);
	PermutationVector.Set<FGenerateRayMarchingTiles::FVoxelCullingDim>(HeterogeneousVolumes::UseSparseVoxelPerTileCulling());

	TShaderRef<FGenerateRayMarchingTiles> ComputeShader = View.ShaderMap->GetShader<FGenerateRayMarchingTiles>(PermutationVector);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("FGenerateRayMarchingTiles"),
		ComputeShader,
		PassParameters,
		GroupCount
	);
}

class FRenderLightingCacheWithPreshadingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRenderLightingCacheWithPreshadingCS);
	SHADER_USE_PARAMETER_STRUCT(FRenderLightingCacheWithPreshadingCS, FGlobalShader);

	class FLightingCacheMode : SHADER_PERMUTATION_INT("DIM_LIGHTING_CACHE_MODE", 2);
	class FUseAdaptiveVolumetricShadowMap : SHADER_PERMUTATION_BOOL("DIM_USE_ADAPTIVE_VOLUMETRIC_SHADOW_MAP");
	class FIndirectLightingMode : SHADER_PERMUTATION_INT("INDIRECT_LIGHTING_MODE", 3);
	using FPermutationDomain = TShaderPermutationDomain<FLightingCacheMode, FUseAdaptiveVolumetricShadowMap, FIndirectLightingMode>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Scene data
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)

		// Light data
		SHADER_PARAMETER(int, bApplyEmissionAndTransmittance)
		SHADER_PARAMETER(int, bApplyDirectLighting)
		SHADER_PARAMETER(int, bApplyShadowTransmittance)
		SHADER_PARAMETER(int, LightType)
		SHADER_PARAMETER_STRUCT_REF(FDeferredLightUniformStruct, DeferredLight)
		SHADER_PARAMETER(float, VolumetricScatteringIntensity)

		// Shadow data
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightUniformParameters, ForwardLightStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeShadowingShaderParameters, VolumeShadowingShaderParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, VirtualShadowMapSamplingParameters)
		SHADER_PARAMETER(int32, VirtualShadowMapId)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FAdaptiveVolumetricShadowMapUniformBufferParameters, AVSM)

		// Global illumination data
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenTranslucencyLightingUniforms, LumenGIVolumeStruct)
		// TODO: Ambient occlusion pipeline
		//SHADER_PARAMETER_RDG_TEXTURE(Texture3D, AmbientOcclusionTexture)
		//SHADER_PARAMETER(FIntVector, AmbientOcclusionResolution)
		SHADER_PARAMETER(float, IndirectInscatteringFactor)

		// Volume structures
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSparseVoxelUniformBufferParameters, SparseVoxelUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLightingCacheParameters, LightingCache)

		// Ray data
		SHADER_PARAMETER(float, MaxShadowTraceDistance)
		SHADER_PARAMETER(float, StepSize)
		SHADER_PARAMETER(int, MipLevel)
		SHADER_PARAMETER(int, MaxStepCount)
		SHADER_PARAMETER(int, bJitter)

		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWLightingCacheTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters
	)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.template Get<FIndirectLightingMode>() == static_cast<int32>(HeterogeneousVolumes::EIndirectLightingMode::SingleScatteringPass))
		{
			return false;
		}

		return DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform);
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (PermutationVector.Get<FIndirectLightingMode>() != static_cast<int32>(HeterogeneousVolumes::EIndirectLightingMode::LightingCachePass))
		{
			PermutationVector.Set<FIndirectLightingMode>(static_cast<int32>(HeterogeneousVolumes::EIndirectLightingMode::Disabled));
		}

		return PermutationVector;
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment
	)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_3D"), GetThreadGroupSize3D());

		bool bSupportVirtualShadowMap = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		if (bSupportVirtualShadowMap)
		{
			OutEnvironment.SetDefine(TEXT("VIRTUAL_SHADOW_MAP"), 1);
			FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		}

		// This shader takes a very long time to compile with FXC, so we pre-compile it with DXC first and then forward the optimized HLSL to FXC.
		//OutEnvironment.CompilerFlags.Add(CFLAG_PrecompileWithDXC);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		OutEnvironment.SetDefine(TEXT("GET_PRIMITIVE_DATA_OVERRIDE"), 1);
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
	static int32 GetThreadGroupSize3D() { return 4; }
};

IMPLEMENT_GLOBAL_SHADER(FRenderLightingCacheWithPreshadingCS, "/Engine/Private/HeterogeneousVolumes/HeterogeneousVolumesPreshadingPipeline.usf", "RenderLightingCacheWithPreshadingCS", SF_Compute);

class FRenderSingleScatteringWithPreshadingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRenderSingleScatteringWithPreshadingCS);
	SHADER_USE_PARAMETER_STRUCT(FRenderSingleScatteringWithPreshadingCS, FGlobalShader);

	class FApplyShadowTransmittanceDim : SHADER_PERMUTATION_BOOL("DIM_APPLY_SHADOW_TRANSMITTANCE");
	class FVoxelCullingDim : SHADER_PERMUTATION_BOOL("DIM_VOXEL_CULLING");
	class FSparseVoxelTracingDim : SHADER_PERMUTATION_BOOL("DIM_SPARSE_VOXEL_TRACING");
	class FFogInscatteringMode : SHADER_PERMUTATION_INT("FOG_INSCATTERING_MODE", 3);
	class FUseInscatteringVolume : SHADER_PERMUTATION_BOOL("DIM_USE_INSCATTERING_VOLUME");
	class FIndirectLightingMode : SHADER_PERMUTATION_INT("INDIRECT_LIGHTING_MODE", 3);
	class FWriteVelocity : SHADER_PERMUTATION_BOOL("DIM_WRITE_VELOCITY");
	class FUseAdaptiveVolumetricShadowMap : SHADER_PERMUTATION_BOOL("DIM_USE_ADAPTIVE_VOLUMETRIC_SHADOW_MAP");
	class FDebugDim : SHADER_PERMUTATION_BOOL("DIM_DEBUG");
	using FPermutationDomain = TShaderPermutationDomain<FApplyShadowTransmittanceDim, FVoxelCullingDim, FSparseVoxelTracingDim, FFogInscatteringMode, FUseInscatteringVolume, FIndirectLightingMode, FWriteVelocity, FUseAdaptiveVolumetricShadowMap, FDebugDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Scene data
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)

		// Light data
		SHADER_PARAMETER(int, bApplyEmissionAndTransmittance)
		SHADER_PARAMETER(int, bApplyDirectLighting)
		SHADER_PARAMETER(int, LightType)
		SHADER_PARAMETER_STRUCT_REF(FDeferredLightUniformStruct, DeferredLight)
		SHADER_PARAMETER(float, VolumetricScatteringIntensity)

		// Shadow data
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightUniformParameters, ForwardLightStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeShadowingShaderParameters, VolumeShadowingShaderParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, VirtualShadowMapSamplingParameters)
		SHADER_PARAMETER(int32, VirtualShadowMapId)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FAdaptiveVolumetricShadowMapUniformBufferParameters, AVSM)

		// Atmosphere
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FFogUniformParameters, FogStruct)

		// Indirect Lighting
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenTranslucencyLightingUniforms, LumenGIVolumeStruct)

		// Volume data
		SHADER_PARAMETER(int, MipLevel)

		// Volume structures
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSparseVoxelUniformBufferParameters, SparseVoxelUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLightingCacheParameters, LightingCache)

		// Ray data
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, StepSize)
		SHADER_PARAMETER(int, MaxStepCount)
		SHADER_PARAMETER(int, bJitter)

		// Ray marching data
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<Volumes::FRayMarchingTile>, RayMarchingTilesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVoxelDataPacked>, VoxelsPerTileBuffer)

		// Indirect args
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER(int32, DownsampleFactor)

		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWLightingTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWVelocityTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVoxelDataPacked>, RWVoxelOutputBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters
	)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FIndirectLightingMode>() == static_cast<int32>(HeterogeneousVolumes::EIndirectLightingMode::LightingCachePass))
		{
			return false;
		}

		return DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform);
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (PermutationVector.Get<FIndirectLightingMode>() != static_cast<int32>(HeterogeneousVolumes::EIndirectLightingMode::SingleScatteringPass))
		{
			PermutationVector.Set<FIndirectLightingMode>(static_cast<int32>(HeterogeneousVolumes::EIndirectLightingMode::Disabled));
		}

		return PermutationVector;
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FDebugDim>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		if (PermutationVector.Get<FVoxelCullingDim>() != HeterogeneousVolumes::UseSparseVoxelPerTileCulling())
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		if (PermutationVector.Get<FSparseVoxelTracingDim>() != HeterogeneousVolumes::UseSparseVoxelPipeline())
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}
		if (PermutationVector.Get<FUseInscatteringVolume>() != HeterogeneousVolumes::UseLightingCacheForInscattering())
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment
	)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());

		bool bSupportVirtualShadowMap = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		if (bSupportVirtualShadowMap)
		{
			OutEnvironment.SetDefine(TEXT("VIRTUAL_SHADOW_MAP"), 1);
			FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		}

		// This shader takes a very long time to compile with FXC, so we pre-compile it with DXC first and then forward the optimized HLSL to FXC.
		//OutEnvironment.CompilerFlags.Add(CFLAG_PrecompileWithDXC);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		OutEnvironment.SetDefine(TEXT("GET_PRIMITIVE_DATA_OVERRIDE"), 1);
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FRenderSingleScatteringWithPreshadingCS, "/Engine/Private/HeterogeneousVolumes/HeterogeneousVolumesPreshadingPipeline.usf", "RenderSingleScatteringWithPreshadingCS", SF_Compute);

void RenderLightingCacheWithPreshadingCompute(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View, int32 ViewIndex,
	const FSceneTextures& SceneTextures,
	// Light data
	bool bApplyEmissionAndTransmittance,
	bool bApplyDirectLighting,
	bool bApplyShadowTransmittance,
	uint32 LightType,
	const FLightSceneInfo* LightSceneInfo,
	// Shadow data
	const FVisibleLightInfo* VisibleLightInfo,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	// Sparse voxel data
	FRDGBufferRef NumVoxelsBuffer,
	const TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters>& SparseVoxelUniformBuffer,
	// Ray marching tiles
	FRDGBufferRef NumRayMarchingTilesBuffer,
	FRDGBufferRef RayMarchingTilesBuffer,
	FRDGBufferRef VoxelsPerTileBuffer,
	// Output
	FRDGTextureRef& LightingCacheTexture
)
{
	// Note must be done in the same scope as we add the pass otherwise the UB lifetime will not be guaranteed
	FDeferredLightUniformStruct DeferredLightUniform = GetDeferredLightParameters(View, *LightSceneInfo);
	TUniformBufferRef<FDeferredLightUniformStruct> DeferredLightUB = CreateUniformBufferImmediate(DeferredLightUniform, UniformBuffer_SingleDraw);

	HeterogeneousVolumes::FLODValue LODValue = HeterogeneousVolumes::CalcLOD(View, HeterogeneousVolumeInterface);

	FRenderLightingCacheWithPreshadingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRenderLightingCacheWithPreshadingCS::FParameters>();
	{
		// Scene data
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures);

		// Light data
		check(LightSceneInfo != nullptr);
		PassParameters->bApplyEmissionAndTransmittance = bApplyEmissionAndTransmittance;
		PassParameters->bApplyDirectLighting = bApplyDirectLighting;
		PassParameters->bApplyShadowTransmittance = bApplyShadowTransmittance;
		PassParameters->DeferredLight = DeferredLightUB;
		PassParameters->LightType = LightType;
		PassParameters->VolumetricScatteringIntensity = LightSceneInfo->Proxy->GetVolumetricScatteringIntensity();

		// Sparse voxel data
		PassParameters->SparseVoxelUniformBuffer = SparseVoxelUniformBuffer;

		// Transmittance volume
		PassParameters->LightingCache.LightingCacheResolution = HeterogeneousVolumes::GetLightingCacheResolution(HeterogeneousVolumeInterface, LODValue);
		PassParameters->LightingCache.LightingCacheVoxelBias = HeterogeneousVolumeInterface->GetShadowBiasFactor();
		PassParameters->LightingCache.LightingCacheTexture = LightingCacheTexture;

		// Ray data
		//PassParameters->StepSize = HeterogeneousVolumes::GetStepSize();
		PassParameters->MaxStepCount = HeterogeneousVolumes::GetMaxStepCount();
		PassParameters->bJitter = HeterogeneousVolumes::ShouldJitter();
		PassParameters->MipLevel = HeterogeneousVolumes::GetMipLevel();

		// Shadow data
		PassParameters->ForwardLightStruct = View.ForwardLightingResources.ForwardLightUniformBuffer;
		if (VisibleLightInfo != nullptr)
		{
			const FProjectedShadowInfo* ProjectedShadowInfo = GetShadowForInjectionIntoVolumetricFog(*VisibleLightInfo);
			bool bDynamicallyShadowed = ProjectedShadowInfo != NULL;
			if (bDynamicallyShadowed)
			{
				GetVolumeShadowingShaderParameters(GraphBuilder, View, LightSceneInfo, ProjectedShadowInfo, PassParameters->VolumeShadowingShaderParameters);
			}
			else
			{
				SetVolumeShadowingDefaultShaderParametersGlobal(GraphBuilder, PassParameters->VolumeShadowingShaderParameters);
			}
			PassParameters->VirtualShadowMapId = VisibleLightInfo->GetVirtualShadowMapId(&View);
		}
		else
		{
			SetVolumeShadowingDefaultShaderParametersGlobal(GraphBuilder, PassParameters->VolumeShadowingShaderParameters);
			PassParameters->VirtualShadowMapId = -1;
		}
		PassParameters->VirtualShadowMapSamplingParameters = VirtualShadowMapArray.GetSamplingParameters(GraphBuilder, ViewIndex);
		PassParameters->AVSM = HeterogeneousVolumes::GetAdaptiveVolumetricShadowMapUniformBuffer(GraphBuilder, View.ViewState, LightSceneInfo);

		// Global illumination data
		auto* LumenUniforms = GraphBuilder.AllocParameters<FLumenTranslucencyLightingUniforms>();
		LumenUniforms->Parameters = GetLumenTranslucencyLightingParameters(GraphBuilder, View.GetLumenTranslucencyGIVolume(), View.LumenFrontLayerTranslucency);
		PassParameters->LumenGIVolumeStruct = GraphBuilder.CreateUniformBuffer(LumenUniforms);
		// TODO: Ambient occlusion pipeline
		//PassParameters->AmbientOcclusionTexture = AmbientOcclusionTexture;
		//PassParameters->AmbientOcclusionResolution = HeterogeneousVolumes::GetAmbientOcclusionResolution(HeterogeneousVolumeInterface, LODValue);
		PassParameters->IndirectInscatteringFactor = HeterogeneousVolumes::GetIndirectLightingFactor();

		// Output
		PassParameters->RWLightingCacheTexture = GraphBuilder.CreateUAV(LightingCacheTexture);
	}

	FString PassName;
#if WANTS_DRAW_MESH_EVENTS
	if (GetEmitDrawEvents())
	{
		FString LightName = TEXT("none");
		if (LightSceneInfo != nullptr)
		{
			FSceneRenderer::GetLightNameForDrawEvent(LightSceneInfo->Proxy, LightName);
		}
		FString ModeName = HeterogeneousVolumes::UseLightingCacheForInscattering() ? TEXT("In-Scattering") : TEXT("Transmittance");
		PassName = FString::Printf(TEXT("RenderLightingCacheWithPreshadingCS [%s] (Light = %s)"), *ModeName, *LightName);
	}
#endif // WANTS_DRAW_MESH_EVENTS
	bool bUseAVSM = HeterogeneousVolumes::UseAdaptiveVolumetricShadowMapForSelfShadowing(HeterogeneousVolumeInterface->GetPrimitiveSceneProxy());
	// Indirect lighting accumulation is coupled with directional light, because it doesn't voxel cull. It is assumed to exist and shadow.
	int32 IndirectLightingMode = LightType == LightType_Directional ? static_cast<int32>(HeterogeneousVolumes::GetIndirectLightingMode()) : 0;

	FRenderLightingCacheWithPreshadingCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRenderLightingCacheWithPreshadingCS::FLightingCacheMode>(HeterogeneousVolumes::GetLightingCacheMode() - 1);
	PermutationVector.Set<FRenderLightingCacheWithPreshadingCS::FUseAdaptiveVolumetricShadowMap>(bUseAVSM);
	PermutationVector.Set<FRenderLightingCacheWithPreshadingCS::FIndirectLightingMode>(IndirectLightingMode);
	PermutationVector = FRenderLightingCacheWithPreshadingCS::RemapPermutation(PermutationVector);
	TShaderRef<FRenderLightingCacheWithPreshadingCS> ComputeShader = View.ShaderMap->GetShader<FRenderLightingCacheWithPreshadingCS>(PermutationVector);

	FIntVector GroupCount = HeterogeneousVolumes::GetLightingCacheResolution(HeterogeneousVolumeInterface, LODValue);
	GroupCount.X = FMath::DivideAndRoundUp(GroupCount.X, FRenderLightingCacheWithPreshadingCS::GetThreadGroupSize3D());
	GroupCount.Y = FMath::DivideAndRoundUp(GroupCount.Y, FRenderLightingCacheWithPreshadingCS::GetThreadGroupSize3D());
	GroupCount.Z = FMath::DivideAndRoundUp(GroupCount.Z, FRenderLightingCacheWithPreshadingCS::GetThreadGroupSize3D());

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("%s", *PassName),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void RenderSingleScatteringWithPreshadingCompute(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View, int32 ViewIndex,
	const FSceneTextures& SceneTextures,
	// Light data
	bool bApplyEmissionAndTransmittance,
	bool bApplyDirectLighting,
	bool bApplyShadowTransmittance,
	uint32 LightType,
	const FLightSceneInfo* LightSceneInfo,
	// Shadow data
	const FVisibleLightInfo* VisibleLightInfo,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	// Sparse voxel data
	FRDGBufferRef NumVoxelsBuffer,
	const TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters>& SparseVoxelUniformBuffer,
	FRDGTextureRef LightingCacheTexture,
	// Ray marching tiles
	FRDGBufferRef NumRayMarchingTilesBuffer,
	FRDGBufferRef RayMarchingTilesBuffer,
	FRDGBufferRef VoxelsPerTileBuffer,
	// Output
	FRDGTextureRef& HeterogeneousVolumeTexture
)
{
	FRDGBufferRef VoxelOutputBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FVoxelDataPacked), HeterogeneousVolumes::GetVoxelCount(SparseVoxelUniformBuffer->GetParameters()->VolumeResolution)),
		TEXT("HeterogeneousVolumes.VoxelOutputBuffer")
	);

	// Note must be done in the same scope as we add the pass otherwise the UB lifetime will not be guaranteed
	FDeferredLightUniformStruct DeferredLightUniform;
	if (bApplyDirectLighting && (LightSceneInfo != nullptr))
	{
		DeferredLightUniform = GetDeferredLightParameters(View, *LightSceneInfo);
	}
	TUniformBufferRef<FDeferredLightUniformStruct> DeferredLightUB = CreateUniformBufferImmediate(DeferredLightUniform, UniformBuffer_SingleDraw);

	bool bWriteVelocity = HeterogeneousVolumes::ShouldWriteVelocity() && HasBeenProduced(SceneTextures.Velocity);
	FRenderSingleScatteringWithPreshadingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRenderSingleScatteringWithPreshadingCS::FParameters>();
	{
		// Scene data
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures);

		// Light data
		PassParameters->bApplyEmissionAndTransmittance = bApplyEmissionAndTransmittance;
		PassParameters->bApplyDirectLighting = bApplyDirectLighting;
		if (PassParameters->bApplyDirectLighting && (LightSceneInfo != nullptr))
		{
			PassParameters->VolumetricScatteringIntensity = LightSceneInfo->Proxy->GetVolumetricScatteringIntensity();
		}
		PassParameters->DeferredLight = DeferredLightUB;
		PassParameters->LightType = LightType;

		// Shadow data
		PassParameters->ForwardLightStruct = View.ForwardLightingResources.ForwardLightUniformBuffer;
		if (VisibleLightInfo != nullptr)
		{
			const FProjectedShadowInfo* ProjectedShadowInfo = GetShadowForInjectionIntoVolumetricFog(*VisibleLightInfo);
			bool bDynamicallyShadowed = ProjectedShadowInfo != NULL;
			if (bDynamicallyShadowed)
			{
				GetVolumeShadowingShaderParameters(GraphBuilder, View, LightSceneInfo, ProjectedShadowInfo, PassParameters->VolumeShadowingShaderParameters);
			}
			else
			{
				SetVolumeShadowingDefaultShaderParametersGlobal(GraphBuilder, PassParameters->VolumeShadowingShaderParameters);
			}
			PassParameters->VirtualShadowMapId = VisibleLightInfo->GetVirtualShadowMapId(&View);
		}
		else
		{
			SetVolumeShadowingDefaultShaderParametersGlobal(GraphBuilder, PassParameters->VolumeShadowingShaderParameters);
			PassParameters->VirtualShadowMapId = -1;
		}
		PassParameters->VirtualShadowMapSamplingParameters = VirtualShadowMapArray.GetSamplingParameters(GraphBuilder, ViewIndex);
		PassParameters->AVSM = HeterogeneousVolumes::GetAdaptiveVolumetricShadowMapUniformBuffer(GraphBuilder, View.ViewState, LightSceneInfo);

		TRDGUniformBufferRef<FFogUniformParameters> FogBuffer = CreateFogUniformBuffer(GraphBuilder, View);
		PassParameters->FogStruct = FogBuffer;

		// Indirect lighting data
		auto* LumenUniforms = GraphBuilder.AllocParameters<FLumenTranslucencyLightingUniforms>();
		LumenUniforms->Parameters = GetLumenTranslucencyLightingParameters(GraphBuilder, View.GetLumenTranslucencyGIVolume(), View.LumenFrontLayerTranslucency);
		PassParameters->LumenGIVolumeStruct = GraphBuilder.CreateUniformBuffer(LumenUniforms);

		// Volume data
		PassParameters->MipLevel = HeterogeneousVolumes::GetMipLevel();

		// Sparse voxel data
		PassParameters->SparseVoxelUniformBuffer = SparseVoxelUniformBuffer;

		// Transmittance volume
		if ((HeterogeneousVolumes::UseLightingCacheForTransmittance() && bApplyShadowTransmittance) || HeterogeneousVolumes::UseLightingCacheForInscattering())
		{
			HeterogeneousVolumes::FLODValue LODValue = HeterogeneousVolumes::CalcLOD(View, HeterogeneousVolumeInterface);
			PassParameters->LightingCache.LightingCacheResolution = HeterogeneousVolumes::GetLightingCacheResolution(HeterogeneousVolumeInterface, LODValue);
			PassParameters->LightingCache.LightingCacheVoxelBias = HeterogeneousVolumeInterface->GetShadowBiasFactor();
			PassParameters->LightingCache.LightingCacheTexture = LightingCacheTexture;
		}
		else
		{
			PassParameters->LightingCache.LightingCacheResolution = FIntVector::ZeroValue;
			PassParameters->LightingCache.LightingCacheVoxelBias = 0.0f;
			PassParameters->LightingCache.LightingCacheTexture = FRDGSystemTextures::Get(GraphBuilder).VolumetricBlack;
		}

		// Ray data
		PassParameters->MaxTraceDistance = HeterogeneousVolumes::GetMaxTraceDistance();
		PassParameters->StepSize = HeterogeneousVolumes::GetStepSize();
		PassParameters->MaxStepCount = HeterogeneousVolumes::GetMaxStepCount();
		PassParameters->bJitter = HeterogeneousVolumes::ShouldJitter();

		// Ray marching data
		PassParameters->RayMarchingTilesBuffer = GraphBuilder.CreateSRV(RayMarchingTilesBuffer);
		PassParameters->VoxelsPerTileBuffer = GraphBuilder.CreateSRV(VoxelsPerTileBuffer);

		// Dispatch data
		PassParameters->IndirectArgs = NumRayMarchingTilesBuffer;
		PassParameters->DownsampleFactor = HeterogeneousVolumes::GetDownsampleFactor();

		// Output
		PassParameters->RWLightingTexture = GraphBuilder.CreateUAV(HeterogeneousVolumeTexture);
		if (bWriteVelocity)
		{
			PassParameters->RWVelocityTexture = GraphBuilder.CreateUAV(SceneTextures.Velocity);
		}
		PassParameters->RWVoxelOutputBuffer = GraphBuilder.CreateUAV(VoxelOutputBuffer);
	}

	FString LightName = TEXT("none");
	if (LightSceneInfo != nullptr)
	{
		FSceneRenderer::GetLightNameForDrawEvent(LightSceneInfo->Proxy, LightName);
	}
	bool bUseAVSM = HeterogeneousVolumes::UseAdaptiveVolumetricShadowMapForSelfShadowing(HeterogeneousVolumeInterface->GetPrimitiveSceneProxy());
	// Indirect lighting accumulation is coupled with directional light, because it doesn't cull voxels. It is assumed to exist and shadow.
	int32 IndirectLightingMode = View.GetLumenTranslucencyGIVolume().Texture0 != nullptr ? static_cast<int32>(HeterogeneousVolumes::GetIndirectLightingMode()) : 0;

	FRenderSingleScatteringWithPreshadingCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRenderSingleScatteringWithPreshadingCS::FApplyShadowTransmittanceDim>(bApplyShadowTransmittance);
	PermutationVector.Set<FRenderSingleScatteringWithPreshadingCS::FVoxelCullingDim>(HeterogeneousVolumes::UseSparseVoxelPerTileCulling());
	PermutationVector.Set<FRenderSingleScatteringWithPreshadingCS::FSparseVoxelTracingDim>(HeterogeneousVolumes::UseSparseVoxelPipeline());
	//PermutationVector.Set<FRenderSingleScatteringWithPreshadingCS::FUseTransmittanceVolume>(HeterogeneousVolumes::UseLightingCacheForTransmittance());
	PermutationVector.Set<FRenderSingleScatteringWithPreshadingCS::FFogInscatteringMode>(static_cast<int32>(HeterogeneousVolumes::GetFogInscatteringMode()));
	PermutationVector.Set<FRenderSingleScatteringWithPreshadingCS::FUseInscatteringVolume>(HeterogeneousVolumes::UseLightingCacheForInscattering());
	PermutationVector.Set<FRenderSingleScatteringWithPreshadingCS::FIndirectLightingMode>(IndirectLightingMode);
	PermutationVector.Set<FRenderSingleScatteringWithPreshadingCS::FWriteVelocity>(bWriteVelocity);
	PermutationVector.Set<FRenderSingleScatteringWithPreshadingCS::FUseAdaptiveVolumetricShadowMap>(bUseAVSM);
	PermutationVector.Set<FRenderSingleScatteringWithPreshadingCS::FDebugDim>(HeterogeneousVolumes::GetDebugMode() != 0);
	PermutationVector = FRenderSingleScatteringWithPreshadingCS::RemapPermutation(PermutationVector);

	TShaderRef<FRenderSingleScatteringWithPreshadingCS> ComputeShader = View.ShaderMap->GetShader<FRenderSingleScatteringWithPreshadingCS>(PermutationVector);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("RenderSingleScatteringWithPreshadingCS (Light = %s)", *LightName),
		ComputeShader,
		PassParameters,
		PassParameters->IndirectArgs,
		0);
}

void RenderWithInscatteringVolumePipelineWithPreshadingCompute(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	const FScene* Scene,
	FViewInfo& View, int32 ViewIndex,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	// Sparse voxel data
	FRDGBufferRef NumVoxelsBuffer,
	TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters>& SparseVoxelUniformBuffer,
	// Render tile data
	FRDGBufferRef NumRayMarchingTilesBuffer,
	FRDGBufferRef RayMarchingTilesBuffer,
	FRDGBufferRef VoxelsPerTileBuffer,
	// Output
	FRDGTextureRef& LightingCacheTexture,
	FRDGTextureRef& HeterogeneousVolumeRadiance
)
{
	RDG_EVENT_SCOPE(GraphBuilder, "Direct Volume Rendering");

	bool bRenderLightingCache = !HeterogeneousVolumes::IsHoldout(HeterogeneousVolumeInterface);
	if (bRenderLightingCache)
	{
		SCOPE_CYCLE_COUNTER(STATGROUP_HeterogeneousVolumesLightCache);

		// Light culling
		TArray<FLightSceneInfoCompact, TInlineAllocator<64>> LightSceneInfoCompact;
		for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
		{
			if (HeterogeneousVolumes::SupportsLightType(LightIt->LightType) &&
				(LightIt->LightSceneInfo->Proxy->GetViewLightingChannelMask() & View.ViewLightingChannelMask) &&
				LightIt->AffectsPrimitive(HeterogeneousVolumeInterface->GetBounds(), HeterogeneousVolumeInterface->GetPrimitiveSceneProxy()))
			{
				LightSceneInfoCompact.Add(*LightIt);
			}
		}

		// Light loop:
		int32 NumPasses = LightSceneInfoCompact.Num();
		for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
		{
			bool bApplyEmissionAndTransmittance = PassIndex == 0;
			bool bApplyDirectLighting = !LightSceneInfoCompact.IsEmpty();
			bool bApplyShadowTransmittance = false;

			uint32 LightType = 0;
			FLightSceneInfo* LightSceneInfo = nullptr;
			const FVisibleLightInfo* VisibleLightInfo = nullptr;
			if (bApplyDirectLighting)
			{
				LightType = LightSceneInfoCompact[PassIndex].LightType;
				LightSceneInfo = LightSceneInfoCompact[PassIndex].LightSceneInfo;
				check(LightSceneInfo != nullptr);

				bApplyDirectLighting = (LightSceneInfo != nullptr);
				if (LightSceneInfo)
				{
					VisibleLightInfo = &VisibleLightInfos[LightSceneInfo->Id];
					bApplyShadowTransmittance = LightSceneInfo->Proxy->CastsVolumetricShadow();
				}
			}

			RenderLightingCacheWithPreshadingCompute(
				GraphBuilder,
				// Scene data
				Scene,
				View, ViewIndex,
				SceneTextures,
				// Light data
				bApplyEmissionAndTransmittance,
				bApplyDirectLighting,
				bApplyShadowTransmittance,
				LightType,
				LightSceneInfo,
				// Shadow data
				VisibleLightInfo,
				VirtualShadowMapArray,
				// Object data
				HeterogeneousVolumeInterface,
				// Volume data
				NumVoxelsBuffer,
				// Sparse voxel data
				SparseVoxelUniformBuffer,
				// Ray marching tile
				NumRayMarchingTilesBuffer,
				RayMarchingTilesBuffer,
				VoxelsPerTileBuffer,
				// Output
				LightingCacheTexture
			);
		}
	}

	// Direct volume integrator
	{
		SCOPE_CYCLE_COUNTER(STATGROUP_HeterogeneousVolumesSingleScattering);

		bool bApplyEmissionAndTransmittance = true;
		bool bApplyDirectLighting = true;
		bool bApplyShadowTransmittance = true;

		uint32 LightType = 0;
		FLightSceneInfo* LightSceneInfo = nullptr;
		const FVisibleLightInfo* VisibleLightInfo = nullptr;

		RenderSingleScatteringWithPreshadingCompute(
			GraphBuilder,
			// Scene
			Scene,
			View, ViewIndex,
			SceneTextures,
			// Light
			bApplyEmissionAndTransmittance,
			bApplyDirectLighting,
			bApplyShadowTransmittance,
			LightType,
			LightSceneInfo,
			// Shadow
			VisibleLightInfo,
			VirtualShadowMapArray,
			// Object
			HeterogeneousVolumeInterface,
			// Volume data
			NumVoxelsBuffer,
			// Sparse voxel data
			SparseVoxelUniformBuffer,
			LightingCacheTexture,
			// Ray marching tile
			NumRayMarchingTilesBuffer,
			RayMarchingTilesBuffer,
			VoxelsPerTileBuffer,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
}

void RenderWithTransmittanceVolumePipelineWithPreshadingCompute(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	const FScene* Scene,
	FViewInfo& View, int32 ViewIndex,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	// Sparse voxel data
	FRDGBufferRef NumVoxelsBuffer,
	TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters>& SparseVoxelUniformBuffer,
	// Render tile data
	FRDGBufferRef NumRayMarchingTilesBuffer,
	FRDGBufferRef RayMarchingTilesBuffer,
	FRDGBufferRef VoxelsPerTileBuffer,
	// Output
	FRDGTextureRef& LightingCacheTexture,
	FRDGTextureRef& HeterogeneousVolumeRadiance
)
{
	RDG_EVENT_SCOPE(GraphBuilder, "Direct Volume Rendering");

	// Light culling
	TArray<FLightSceneInfoCompact, TInlineAllocator<64>> LightSceneInfoCompact;
	for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
	{
		if (HeterogeneousVolumes::SupportsLightType(LightIt->LightType) &&
			(LightIt->LightSceneInfo->Proxy->GetViewLightingChannelMask() & View.ViewLightingChannelMask) &&
			LightIt->AffectsPrimitive(HeterogeneousVolumeInterface->GetBounds(), HeterogeneousVolumeInterface->GetPrimitiveSceneProxy()))
		{
			LightSceneInfoCompact.Add(*LightIt);
		}
	}

	// Single-scattering
	int32 NumPasses = FMath::Max(LightSceneInfoCompact.Num(), 1);
	for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		bool bApplyEmissionAndTransmittance = PassIndex == 0;
		bool bApplyDirectLighting = !LightSceneInfoCompact.IsEmpty();
		bool bApplyShadowTransmittance = false;

		uint32 LightType = 0;
		FLightSceneInfo* LightSceneInfo = nullptr;
		const FVisibleLightInfo* VisibleLightInfo = nullptr;
		if (bApplyDirectLighting)
		{
			LightType = LightSceneInfoCompact[PassIndex].LightType;
			LightSceneInfo = LightSceneInfoCompact[PassIndex].LightSceneInfo;
			check(LightSceneInfo != nullptr);

			bApplyDirectLighting = (LightSceneInfo != nullptr);
			if (LightSceneInfo)
			{
				VisibleLightInfo = &VisibleLightInfos[LightSceneInfo->Id];
				bApplyShadowTransmittance = LightSceneInfo->Proxy->CastsVolumetricShadow();
			}
		}

		if (HeterogeneousVolumes::UseLightingCacheForTransmittance() && bApplyShadowTransmittance)
		{
			RenderLightingCacheWithPreshadingCompute(
				GraphBuilder,
				// Scene data
				Scene,
				View, ViewIndex,
				SceneTextures,
				// Light data
				bApplyEmissionAndTransmittance,
				bApplyDirectLighting,
				bApplyShadowTransmittance,
				LightType,
				LightSceneInfo,
				// Shadow data
				VisibleLightInfo,
				VirtualShadowMapArray,
				// Object data
				HeterogeneousVolumeInterface,
				// Volume data
				NumVoxelsBuffer,
				// Sparse voxel data
				SparseVoxelUniformBuffer,
				// Ray marching tile
				NumRayMarchingTilesBuffer,
				RayMarchingTilesBuffer,
				VoxelsPerTileBuffer,
				// Output
				LightingCacheTexture
			);
		}

		RenderSingleScatteringWithPreshadingCompute(
			GraphBuilder,
			// Scene
			Scene,
			View, ViewIndex,
			SceneTextures,
			// Light
			bApplyEmissionAndTransmittance,
			bApplyDirectLighting,
			bApplyShadowTransmittance,
			LightType,
			LightSceneInfo,
			// Shadow
			VisibleLightInfo,
			VirtualShadowMapArray,
			// Object
			HeterogeneousVolumeInterface,
			// Volume data
			NumVoxelsBuffer,
			// Sparse voxel data
			SparseVoxelUniformBuffer,
			LightingCacheTexture,
			// Ray marching tile
			NumRayMarchingTilesBuffer,
			RayMarchingTilesBuffer,
			VoxelsPerTileBuffer,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
}

void RenderWithPreshadingCompute(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	const FScene* Scene,
	FViewInfo& View, int32 ViewIndex,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	// Sparse voxel data
	FRDGBufferRef NumVoxelsBuffer,
	TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters>& SparseVoxelUniformBuffer,
	// Output
	FRDGTextureRef& LightingCacheTexture,
	FRDGTextureRef& HeterogeneousVolumeRadiance
)
{
	RDG_EVENT_SCOPE(GraphBuilder, "Software Ray Tracing");

	FRDGBufferRef NumRayMarchingTilesBuffer;
	FRDGBufferRef RayMarchingTilesBuffer;
	FRDGBufferRef VoxelsPerTileBuffer;
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Ray Tile Generation");
		GenerateRayMarchingTiles(
			GraphBuilder,
			// Scene
			Scene,
			View,
			SceneTextures,
			// Object
			HeterogeneousVolumeInterface,
			// Volume data
			NumVoxelsBuffer,
			// Sparse voxel data
			SparseVoxelUniformBuffer,
			// Output
			NumRayMarchingTilesBuffer,
			RayMarchingTilesBuffer,
			VoxelsPerTileBuffer
		);
	}

	if (HeterogeneousVolumes::UseLightingCacheForInscattering())
	{
		RenderWithInscatteringVolumePipelineWithPreshadingCompute(
			GraphBuilder,
			// Scene data
			SceneTextures,
			Scene,
			View, ViewIndex,
			// Shadow data
			VisibleLightInfos,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			// Sparse voxel data
			NumVoxelsBuffer,
			SparseVoxelUniformBuffer,
			// Render tile data
			NumRayMarchingTilesBuffer,
			RayMarchingTilesBuffer,
			VoxelsPerTileBuffer,
			// Output
			LightingCacheTexture,
			HeterogeneousVolumeRadiance
		);
	}
	else
	{
		RenderWithTransmittanceVolumePipelineWithPreshadingCompute(
			GraphBuilder,
			// Scene data
			SceneTextures,
			Scene,
			View, ViewIndex,
			// Shadow data
			VisibleLightInfos,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			// Sparse voxel data
			NumVoxelsBuffer,
			SparseVoxelUniformBuffer,
			// Render tile data
			NumRayMarchingTilesBuffer,
			RayMarchingTilesBuffer,
			VoxelsPerTileBuffer,
			// Output
			LightingCacheTexture,
			HeterogeneousVolumeRadiance
		);
	}
}

void RenderWithInscatteringVolumePipelineWithPreshadingHardwareRayTracing(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	FScene* Scene,
	FViewInfo& View, int32 ViewIndex,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	// Sparse voxel data
	FRDGBufferRef NumVoxelsBuffer,
	const TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters>& SparseVoxelUniformBuffer,
	// Transmittance acceleration
	FRDGTextureRef LightingCacheTexture,
	// Ray tracing data
	TConstArrayView<FRayTracingGeometryRHIRef> RayTracingGeometries,
	// Output
	FRDGTextureRef& HeterogeneousVolumeRadiance
)
{
#if RHI_RAYTRACING
	RDG_EVENT_SCOPE(GraphBuilder, "Direct Volume Rendering");

	// Light culling
	TArray<FLightSceneInfoCompact, TInlineAllocator<64>> LightSceneInfoCompact;
	for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
	{
		if (HeterogeneousVolumes::SupportsLightType(LightIt->LightType) &&
			(LightIt->LightSceneInfo->Proxy->GetViewLightingChannelMask() & View.ViewLightingChannelMask) &&
			LightIt->AffectsPrimitive(HeterogeneousVolumeInterface->GetBounds(), HeterogeneousVolumeInterface->GetPrimitiveSceneProxy()))
		{
			LightSceneInfoCompact.Add(*LightIt);
		}
	}

	// Single-scattering
	int32 NumPasses = FMath::Max(LightSceneInfoCompact.Num(), 1);
	for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		bool bApplyEmissionAndTransmittance = PassIndex == 0;
		bool bApplyDirectLighting = !LightSceneInfoCompact.IsEmpty();
		bool bApplyShadowTransmittance = false;

		uint32 LightType = 0;
		FLightSceneInfo* LightSceneInfo = nullptr;
		const FVisibleLightInfo* VisibleLightInfo = nullptr;
		if (bApplyDirectLighting)
		{
			LightType = LightSceneInfoCompact[PassIndex].LightType;
			LightSceneInfo = LightSceneInfoCompact[PassIndex].LightSceneInfo;
			check(LightSceneInfo != nullptr);

			bApplyDirectLighting = (LightSceneInfo != nullptr);
			if (LightSceneInfo)
			{
				VisibleLightInfo = &VisibleLightInfos[LightSceneInfo->Id];
				bApplyShadowTransmittance = LightSceneInfo->Proxy->CastsVolumetricShadow();
			}
		}

		RenderLightingCacheWithPreshadingHardwareRayTracing(
			GraphBuilder,
			// Scene data
			Scene,
			View, ViewIndex,
			SceneTextures,
			// Light data
			bApplyEmissionAndTransmittance,
			bApplyDirectLighting,
			bApplyShadowTransmittance,
			LightType,
			LightSceneInfo,
			// Shadow
			VisibleLightInfo,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			// Sparse voxel
			SparseVoxelUniformBuffer,
			// Ray tracing data
			Scene->HeterogeneousVolumesRayTracingScene,
			RayTracingGeometries,
			// Transmittance volume
			LightingCacheTexture
		);
	}

	// Direct volume integrator
	{
		bool bApplyEmissionAndTransmittance = true;
		bool bApplyDirectLighting = true;
		bool bApplyShadowTransmittance = true;

		uint32 LightType = 0;
		FLightSceneInfo* LightSceneInfo = nullptr;
		const FVisibleLightInfo* VisibleLightInfo = nullptr;

		RenderSingleScatteringWithPreshadingHardwareRayTracing(
			GraphBuilder,
			// Scene data
			Scene,
			View, ViewIndex,
			SceneTextures,
			// Light data
			bApplyEmissionAndTransmittance,
			bApplyDirectLighting,
			bApplyShadowTransmittance,
			LightType,
			LightSceneInfo,
			// Shadow
			VisibleLightInfo,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			// Sparse voxel
			SparseVoxelUniformBuffer,
			// Ray tracing data
			Scene->HeterogeneousVolumesRayTracingScene,
			RayTracingGeometries,
			// Transmittance volume
			LightingCacheTexture,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
#endif // RHI_RAYTRACING
}

void RenderWithTransmittanceVolumePipelineWithPreshadingHardwareRayTracing(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	FScene* Scene,
	FViewInfo& View, int32 ViewIndex,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	// Sparse voxel data
	FRDGBufferRef NumVoxelsBuffer,
	const TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters>& SparseVoxelUniformBuffer,
	// Transmittance acceleration
	FRDGTextureRef LightingCacheTexture,
	// Ray tracing data
	TConstArrayView<FRayTracingGeometryRHIRef> RayTracingGeometries,
	// Output
	FRDGTextureRef& HeterogeneousVolumeRadiance
)
{
#if RHI_RAYTRACING
	RDG_EVENT_SCOPE(GraphBuilder, "Direct Volume Rendering");

	// Light culling
	TArray<FLightSceneInfoCompact, TInlineAllocator<64>> LightSceneInfoCompact;
	for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
	{
		if (HeterogeneousVolumes::SupportsLightType(LightIt->LightType) &&
			(LightIt->LightSceneInfo->Proxy->GetViewLightingChannelMask() & View.ViewLightingChannelMask) &&
			LightIt->AffectsPrimitive(HeterogeneousVolumeInterface->GetBounds(), HeterogeneousVolumeInterface->GetPrimitiveSceneProxy()))
		{
			LightSceneInfoCompact.Add(*LightIt);
		}
	}

	// Single-scattering
	int32 NumPasses = FMath::Max(LightSceneInfoCompact.Num(), 1);
	for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		bool bApplyEmissionAndTransmittance = PassIndex == 0;
		bool bApplyDirectLighting = !LightSceneInfoCompact.IsEmpty();
		bool bApplyShadowTransmittance = false;

		uint32 LightType = 0;
		FLightSceneInfo* LightSceneInfo = nullptr;
		const FVisibleLightInfo* VisibleLightInfo = nullptr;
		if (bApplyDirectLighting)
		{
			LightType = LightSceneInfoCompact[PassIndex].LightType;
			LightSceneInfo = LightSceneInfoCompact[PassIndex].LightSceneInfo;
			check(LightSceneInfo != nullptr);

			bApplyDirectLighting = (LightSceneInfo != nullptr);
			if (LightSceneInfo)
			{
				VisibleLightInfo = &VisibleLightInfos[LightSceneInfo->Id];
				bApplyShadowTransmittance = LightSceneInfo->Proxy->CastsVolumetricShadow();
			}
		}

		if (HeterogeneousVolumes::UseLightingCacheForTransmittance() && bApplyShadowTransmittance)
		{
			RenderLightingCacheWithPreshadingHardwareRayTracing(
				GraphBuilder,
				// Scene data
				Scene,
				View, ViewIndex,
				SceneTextures,
				// Light data
				bApplyEmissionAndTransmittance,
				bApplyDirectLighting,
				bApplyShadowTransmittance,
				LightType,
				LightSceneInfo,
				// Shadow
				VisibleLightInfo,
				VirtualShadowMapArray,
				// Object data
				HeterogeneousVolumeInterface,
				// Sparse voxel
				SparseVoxelUniformBuffer,
				// Ray tracing data
				Scene->HeterogeneousVolumesRayTracingScene,
				RayTracingGeometries,
				// Transmittance volume
				LightingCacheTexture
			);
		}

		RenderSingleScatteringWithPreshadingHardwareRayTracing(
			GraphBuilder,
			// Scene data
			Scene,
			View, ViewIndex,
			SceneTextures,
			// Light data
			bApplyEmissionAndTransmittance,
			bApplyDirectLighting,
			bApplyShadowTransmittance,
			LightType,
			LightSceneInfo,
			// Shadow
			VisibleLightInfo,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			// Sparse voxel
			SparseVoxelUniformBuffer,
			// Ray tracing data
			Scene->HeterogeneousVolumesRayTracingScene,
			RayTracingGeometries,
			// Transmittance volume
			LightingCacheTexture,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
#endif // RHI_RAYTRACING
}

void RenderWithPreshadingHardwareRayTracing(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	FScene* Scene,
	FViewInfo& View, int32 ViewIndex,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	// Sparse voxel data
	FRDGBufferRef NumVoxelsBuffer,
	const TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters>& SparseVoxelUniformBuffer,
	// Transmittance acceleration
	FRDGTextureRef LightingCacheTexture,
	// Output
	FRDGTextureRef& HeterogeneousVolumeRadiance
)
{
#if RHI_RAYTRACING
	RDG_EVENT_SCOPE(GraphBuilder, "Hardware Ray Tracing");

	// WARNING: Currently works, but I'm skeptical if all RHI resources have the correct lifetime management
	TArray<FRayTracingGeometryRHIRef, SceneRenderingAllocator> RayTracingGeometries = GraphBuilder.AllocArray<FRayTracingGeometryRHIRef>();
	TArray<FMatrix> RayTracingTransforms;
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Acceleration Structure Build");

		GenerateRayTracingGeometryInstance(
			GraphBuilder,
			// Scene
			Scene,
			View,
			// Object
			HeterogeneousVolumeInterface,
			// Sparse voxel
			NumVoxelsBuffer,
			SparseVoxelUniformBuffer,
			// Output
			RayTracingGeometries,
			RayTracingTransforms
		);

		GenerateRayTracingScene(
			GraphBuilder,
			// Scene
			Scene,
			View,
			// Ray tracing data
			RayTracingGeometries,
			RayTracingTransforms,
			// Output
			Scene->HeterogeneousVolumesRayTracingScene
		);
	}

	if (HeterogeneousVolumes::UseLightingCacheForInscattering())
	{
		RenderWithInscatteringVolumePipelineWithPreshadingHardwareRayTracing(
			GraphBuilder,
			SceneTextures,
			Scene,
			View, ViewIndex,
			// Shadow data
			VisibleLightInfos,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			NumVoxelsBuffer,
			SparseVoxelUniformBuffer,
			// Transmittance acceleration
			LightingCacheTexture,
			// Ray tracing data
			RayTracingGeometries,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
	else
	{
		RenderWithTransmittanceVolumePipelineWithPreshadingHardwareRayTracing(
			GraphBuilder,
			SceneTextures,
			Scene,
			View, ViewIndex,
			// Shadow data
			VisibleLightInfos,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			NumVoxelsBuffer,
			SparseVoxelUniformBuffer,
			// Transmittance acceleration
			LightingCacheTexture,
			// Ray tracing data
			RayTracingGeometries,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
#endif // RHI_RAYTRACING
}

class FGenerateMips3D : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateMips3D);
	SHADER_USE_PARAMETER_STRUCT(FGenerateMips3D, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment
	)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_3D"), GetThreadGroupSize3D());

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
		//SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, InputTexture)
		SHADER_PARAMETER(FIntVector, TextureResolution)
		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWOutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetThreadGroupSize3D() { return 4; }
};

IMPLEMENT_GLOBAL_SHADER(FGenerateMips3D, "/Engine/Private/HeterogeneousVolumes/HeterogeneousVolumesPreshadingPipeline.usf", "GenerateMips3D", SF_Compute);

class FGenerateMin3D : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateMin3D);
	SHADER_USE_PARAMETER_STRUCT(FGenerateMin3D, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment
	)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_3D"), GetThreadGroupSize3D());

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
		//SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, InputTexture)
		SHADER_PARAMETER(FIntVector, TextureResolution)
		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWOutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetThreadGroupSize3D() { return 4; }
};

IMPLEMENT_GLOBAL_SHADER(FGenerateMin3D, "/Engine/Private/HeterogeneousVolumes/HeterogeneousVolumesPreshadingPipeline.usf", "GenerateMin3D", SF_Compute);

template<typename ShaderType>
void GenerateMips3D(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef Texture,
	uint32 MipLevel
)
{
	FRDGTextureDesc TextureDesc = Texture->Desc;
	const FIntVector TextureResolution(
		FMath::Max(TextureDesc.Extent.X >> MipLevel, 1),
		FMath::Max(TextureDesc.Extent.Y >> MipLevel, 1),
		FMath::Max(TextureDesc.Depth >> MipLevel, 1));

	typename ShaderType::FParameters* PassParameters = GraphBuilder.AllocParameters<typename ShaderType::FParameters>();
	{
		PassParameters->TextureResolution = TextureResolution;
		PassParameters->InputTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(Texture, MipLevel - 1));
		PassParameters->TextureSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		//PassParameters->InputTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Texture, MipLevel - 1));
		PassParameters->RWOutputTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Texture, MipLevel));
	}

	uint32 GroupCountX = FMath::DivideAndRoundUp(TextureResolution.X, ShaderType::GetThreadGroupSize3D());
	uint32 GroupCountY = FMath::DivideAndRoundUp(TextureResolution.Y, ShaderType::GetThreadGroupSize3D());
	uint32 GroupCountZ = FMath::DivideAndRoundUp(TextureResolution.Z, ShaderType::GetThreadGroupSize3D());
	FIntVector GroupCount(GroupCountX, GroupCountY, GroupCountZ);

	TShaderRef<ShaderType> ComputeShader = View.ShaderMap->GetShader<ShaderType>();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("FGenerateMips3D"),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void RenderWithPreshading(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	FScene* Scene,
	FViewInfo& View, int32 ViewIndex,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FPersistentPrimitiveIndex &PersistentPrimitiveIndex,
	const FBoxSphereBounds LocalBoxSphereBounds,
	// Transmittance acceleration
	FRDGTextureRef LightingCacheTexture,
	// Output
	FRDGTextureRef& HeterogeneousVolumeRadiance
)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);

	// Determine baking voxel resolution
	float LODFactor = HeterogeneousVolumes::CalcLODFactor(View, HeterogeneousVolumeInterface);
	FIntVector VolumeResolution = HeterogeneousVolumes::GetVolumeResolution(HeterogeneousVolumeInterface);
	// TODO: Modify volume resolution by LODFactor??

	// Create baked material grids
	uint32 NumMips = FMath::Log2(float(FMath::Min(FMath::Min(VolumeResolution.X, VolumeResolution.Y), VolumeResolution.Z))) + 1;
	FRDGTextureDesc BakedMaterialDesc = FRDGTextureDesc::Create3D(
		VolumeResolution,
		PF_FloatR11G11B10,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV | TexCreate_3DTiling,
		NumMips
	);

	FRDGTextureRef ExtinctionTexture = GraphBuilder.CreateTexture(BakedMaterialDesc, TEXT("HeterogeneousVolumes.ExtinctionTexture"));
	FRDGTextureRef EmissionTexture = GraphBuilder.CreateTexture(BakedMaterialDesc, TEXT("HeterogeneousVolumes.EmissionTexture"));
	FRDGTextureRef AlbedoTexture = GraphBuilder.CreateTexture(BakedMaterialDesc, TEXT("HeterogeneousVolumes.AlbedoTexture"));

	// Preshading pipeline
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Preshading Pipeline");
		SCOPE_CYCLE_COUNTER(STATGROUP_HeterogeneousVolumesMaterialBaking);

		{
			RDG_EVENT_SCOPE(GraphBuilder, "Material Baking");

			ComputeHeterogeneousVolumeBakeMaterial(
				GraphBuilder,
				// Scene data
				Scene,
				View,
				// Object data
				HeterogeneousVolumeInterface,
				MaterialRenderProxy,
				PersistentPrimitiveIndex,
				LocalBoxSphereBounds,
				// Volume data
				VolumeResolution,
				// Output
				ExtinctionTexture,
				EmissionTexture,
				AlbedoTexture
			);
		}

		// MIP Generation
		{
			RDG_EVENT_SCOPE(GraphBuilder, "MIP Generation");
			for (uint32 MipLevel = 1; MipLevel < NumMips; ++MipLevel)
			{
				GenerateMips3D<FGenerateMips3D>(GraphBuilder, View, ExtinctionTexture, MipLevel);
				// TODO: Reinstate once ray-marching determines appropriate MIP level to sample
				//GenerateMips3D<FGenerateMips3D>(GraphBuilder, View, EmissionTexture, MipLevel);
				//GenerateMips3D<FGenerateMips3D>(GraphBuilder, View, AlbedoTexture, MipLevel);
			}
		}
	}

	// Sparse Voxel Pipeline
	int32 MipBias = HeterogeneousVolumes::GetSparseVoxelMipBias();
	uint32 SparseMipLevel = FMath::Clamp(int32(NumMips) - MipBias, 0, int32(NumMips) - 1);

	FRDGTextureRef MinTexture;
	FRDGBufferRef NumVoxelsBuffer;
	FRDGBufferRef VoxelBuffer;
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Sparse Voxel Pipeline");
		SCOPE_CYCLE_COUNTER(STATGROUP_HeterogeneousVolumesMaterialBaking);

		{
			RDG_EVENT_SCOPE(GraphBuilder, "Min Generation");

			FRDGTextureDesc MinTextureDesc = ExtinctionTexture->Desc;
			MinTextureDesc.Extent.X = FMath::Max(MinTextureDesc.Extent.X >> SparseMipLevel, 1);
			MinTextureDesc.Extent.Y = FMath::Max(MinTextureDesc.Extent.Y >> SparseMipLevel, 1);
			MinTextureDesc.Depth = FMath::Max(MinTextureDesc.Depth >> SparseMipLevel, 1);
			MinTextureDesc.NumMips = FMath::Log2(float(FMath::Min(FMath::Min(MinTextureDesc.Extent.X, MinTextureDesc.Extent.Y), (int32)MinTextureDesc.Depth))) + 1;

			MinTexture = GraphBuilder.CreateTexture(MinTextureDesc, TEXT("HeterogeneousVolumes.MinTexture"));
			CopyTexture3D(GraphBuilder, View, ExtinctionTexture, SparseMipLevel, MinTexture);
			for (uint32 MipLevel = 1; MipLevel < MinTextureDesc.NumMips; ++MipLevel)
			{
				GenerateMips3D<FGenerateMin3D>(GraphBuilder, View, MinTexture, MipLevel);
			}
		}

		{
			RDG_EVENT_SCOPE(GraphBuilder, "Sparse Voxel Generation");
			GenerateSparseVoxels(GraphBuilder, View, MinTexture, VolumeResolution, SparseMipLevel, NumVoxelsBuffer, VoxelBuffer);
		}
	}

	// Create Sparse Voxel UniformBuffer
	FSparseVoxelUniformBufferParameters* SparseVoxelUniformBufferParameters = GraphBuilder.AllocParameters<FSparseVoxelUniformBufferParameters>();
	{
		// Object data
		// TODO: Convert to relative-local space
		//FVector3f ViewOriginHigh = FDFVector3(View.ViewMatrices.GetViewOrigin()).High;
		//FMatrix44f RelativeLocalToWorld = FDFMatrix::MakeToRelativeWorldMatrix(ViewOriginHigh, HeterogeneousVolumeInterface->GetLocalToWorld()).M;
		FMatrix InstanceToLocal = HeterogeneousVolumeInterface->GetInstanceToLocal();
		FMatrix LocalToWorld = HeterogeneousVolumeInterface->GetLocalToWorld();
		SparseVoxelUniformBufferParameters->LocalToWorld = FMatrix44f(InstanceToLocal * LocalToWorld);
		SparseVoxelUniformBufferParameters->WorldToLocal = SparseVoxelUniformBufferParameters->LocalToWorld.Inverse();

		FMatrix LocalToInstance = InstanceToLocal.Inverse();
		FBoxSphereBounds InstanceBoxSphereBounds = LocalBoxSphereBounds.TransformBy(LocalToInstance);
		SparseVoxelUniformBufferParameters->LocalBoundsOrigin = FVector3f(InstanceBoxSphereBounds.Origin);
		SparseVoxelUniformBufferParameters->LocalBoundsExtent = FVector3f(InstanceBoxSphereBounds.BoxExtent);

		// Volume data
		SparseVoxelUniformBufferParameters->VolumeResolution = VolumeResolution;
		SparseVoxelUniformBufferParameters->ExtinctionTexture = ExtinctionTexture;
		SparseVoxelUniformBufferParameters->EmissionTexture = EmissionTexture;
		SparseVoxelUniformBufferParameters->AlbedoTexture = AlbedoTexture;
		SparseVoxelUniformBufferParameters->TextureSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// Sparse voxel data
		SparseVoxelUniformBufferParameters->NumVoxelsBuffer = GraphBuilder.CreateSRV(NumVoxelsBuffer, PF_R32_UINT);
		SparseVoxelUniformBufferParameters->VoxelBuffer = GraphBuilder.CreateSRV(VoxelBuffer);
		SparseVoxelUniformBufferParameters->MipLevel = SparseMipLevel;

		// Traversal hints
		SparseVoxelUniformBufferParameters->MaxTraceDistance = HeterogeneousVolumes::GetMaxTraceDistance();
		SparseVoxelUniformBufferParameters->MaxShadowTraceDistance = HeterogeneousVolumes::GetMaxShadowTraceDistance();
		SparseVoxelUniformBufferParameters->StepSize = HeterogeneousVolumes::GetStepSize();
		SparseVoxelUniformBufferParameters->StepFactor = HeterogeneousVolumeInterface->GetStepFactor() * LODFactor;
		SparseVoxelUniformBufferParameters->ShadowStepSize = HeterogeneousVolumes::GetShadowStepSize();
		SparseVoxelUniformBufferParameters->ShadowStepFactor = HeterogeneousVolumeInterface->GetShadowStepFactor() * LODFactor;
		SparseVoxelUniformBufferParameters->bApplyHeightFog = HeterogeneousVolumes::ShouldApplyHeightFog();
		SparseVoxelUniformBufferParameters->bApplyVolumetricFog = HeterogeneousVolumes::ShouldApplyVolumetricFog();
		SparseVoxelUniformBufferParameters->IndirectInscatteringFactor = HeterogeneousVolumes::GetIndirectLightingFactor();
	}
	TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters> SparseVoxelUniformBuffer = GraphBuilder.CreateUniformBuffer(SparseVoxelUniformBufferParameters);

	// Hardware ray tracing
	if (HeterogeneousVolumes::UseHardwareRayTracing())
	{
		RenderWithPreshadingHardwareRayTracing(
			GraphBuilder,
			// Scene data
			SceneTextures,
			Scene,
			View, ViewIndex,
			// Shadow data
			VisibleLightInfos,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			// Sparse voxel data
			NumVoxelsBuffer,
			SparseVoxelUniformBuffer,
			// Transmittance acceleration
			LightingCacheTexture,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
	// Software ray tracing
	else
	{
		RenderWithPreshadingCompute(
			GraphBuilder,
			// Scene data
			SceneTextures,
			Scene,
			View, ViewIndex,
			// Shadow data
			VisibleLightInfos,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			// Sparse voxel data
			NumVoxelsBuffer,
			SparseVoxelUniformBuffer,
			// Transmittance acceleration
			LightingCacheTexture,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
}