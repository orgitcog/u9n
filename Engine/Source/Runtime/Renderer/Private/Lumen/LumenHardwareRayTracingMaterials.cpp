// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "RenderCore.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RayTracing/RayTracingScene.h"
#include "RayTracing/RayTracingMaterialHitShaders.h"
#include "RayTracing/RayTracing.h"
#include "RenderGraphUtils.h"
#include "DeferredShadingRenderer.h"
#include "PipelineStateCache.h"
#include "ScenePrivate.h"
#include "ShaderCompilerCore.h"
#include "Lumen/LumenHardwareRayTracingCommon.h"
#include "Nanite/NaniteRayTracing.h"
#include "Lumen/LumenReflections.h"
#include "Lumen/RayTracedTranslucency.h"

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingSkipBackFaceHitDistance(
	TEXT("r.Lumen.HardwareRayTracing.SkipBackFaceHitDistance"),
	5.0f,
	TEXT("Distance to trace with backface culling enabled, useful when the Ray Tracing geometry doesn't match the GBuffer (Nanite Proxy geometry)."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingSkipTwoSidedHitDistance(
	TEXT("r.Lumen.HardwareRayTracing.SkipTwoSidedHitDistance"),
	1.0f,
	TEXT("When the SkipBackFaceHitDistance is enabled, the first two-sided material hit within this distance will be skipped. This is useful for avoiding self-intersections with the Nanite fallback mesh on foliage, as SkipBackFaceHitDistance doesn't work on two sided materials."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace LumenHardwareRayTracing
{
	// 0 - hit group with EAvoidSelfIntersectionsMode::Disabled
	// 1 - hit group with EAvoidSelfIntersectionsMode::AHS
	constexpr uint32 NumHitGroups = 2;
};

IMPLEMENT_RT_PAYLOAD_TYPE(ERayTracingPayloadType::LumenMinimal, 16);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FLumenHardwareRayTracingUniformBufferParameters, "LumenHardwareRayTracingUniformBuffer");

class FLumenHardwareRayTracingMaterialHitGroup : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialHitGroup)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FLumenHardwareRayTracingMaterialHitGroup, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FLumenHardwareRayTracingUniformBufferParameters, LumenHardwareRayTracingUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_REF(FNaniteRayTracingUniformParameters, NaniteRayTracing)
		SHADER_PARAMETER_STRUCT_REF(FSceneUniformParameters, Scene)
	END_SHADER_PARAMETER_STRUCT()

	class FAvoidSelfIntersectionsMode : SHADER_PERMUTATION_ENUM_CLASS("AVOID_SELF_INTERSECTIONS_MODE", LumenHardwareRayTracing::EAvoidSelfIntersectionsMode);
	class FNaniteRayTracing : SHADER_PERMUTATION_BOOL("NANITE_RAY_TRACING");
	using FPermutationDomain = TShaderPermutationDomain<FAvoidSelfIntersectionsMode, FNaniteRayTracing>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform) 
			&& (DoesPlatformSupportLumenGI(Parameters.Platform) || MegaLights::ShouldCompileShaders(Parameters.Platform));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}

	static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Parameters) 
	{
		return RayTracing::GetShaderBindingLayout(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialHitGroup, "/Engine/Private/Lumen/LumenHardwareRayTracingMaterials.usf", "closesthit=LumenHardwareRayTracingMaterialCHS anyhit=LumenHardwareRayTracingMaterialAHS", SF_RayHitGroup);

class FLumenHardwareRayTracingMaterialMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialMS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FLumenHardwareRayTracingMaterialMS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform) 
			&& (DoesPlatformSupportLumenGI(Parameters.Platform) || MegaLights::ShouldCompileShaders(Parameters.Platform));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}
	
	static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Parameters) 
	{
		return RayTracing::GetShaderBindingLayout(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

IMPLEMENT_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialMS, "/Engine/Private/Lumen/LumenHardwareRayTracingMaterials.usf", "LumenHardwareRayTracingMaterialMS", SF_RayMiss);

void FDeferredShadingSceneRenderer::SetupLumenHardwareRayTracingUniformBuffer(FViewInfo& View)
{
	FLumenHardwareRayTracingUniformBufferParameters LumenHardwareRayTracingUniformBufferParameters;
	LumenHardwareRayTracingUniformBufferParameters.SkipBackFaceHitDistance	= CVarLumenHardwareRayTracingSkipBackFaceHitDistance.GetValueOnRenderThread();
	LumenHardwareRayTracingUniformBufferParameters.SkipTwoSidedHitDistance	= CVarLumenHardwareRayTracingSkipTwoSidedHitDistance.GetValueOnRenderThread();
	LumenHardwareRayTracingUniformBufferParameters.SkipTranslucent			= LumenReflections::UseTranslucentRayTracing(View) || RayTracedTranslucency::IsEnabled(View) ? 0.0f : 1.0f;
	LumenHardwareRayTracingUniformBufferParameters.DiffuseColorBoost		= 1.0f / FMath::Max(View.FinalPostProcessSettings.LumenDiffuseColorBoost, 1.0f);
	View.LumenHardwareRayTracingUniformBuffer = TUniformBufferRef<FLumenHardwareRayTracingUniformBufferParameters>::CreateUniformBufferImmediate(LumenHardwareRayTracingUniformBufferParameters, UniformBuffer_SingleFrame);
}

uint32 CalculateLumenHardwareRayTracingUserData(const FRayTracingShaderBindingData& RTShaderBinding, const FRayTracingMeshCommand& MeshCommand)
{
	const bool bDynamicGeometry = RTShaderBinding.RayTracingGeometry->GetInitializer().bAllowUpdate;
	return (MeshCommand.MaterialShaderIndex & LUMEN_MATERIAL_SHADER_INDEX_MASK)
		| (((bDynamicGeometry != 0) & 0x01) << 27)
		| (((MeshCommand.bAlphaMasked != 0) & 0x01) << 28)
		| (((MeshCommand.bCastRayTracedShadows != 0) & 0x01) << 29)
		| (((MeshCommand.bTwoSided != 0) & 0x01) << 30)
		| (((MeshCommand.bIsTranslucent != 0) & 0x01) << 31);
}

// TODO: This should be moved into FRayTracingScene and used as a base for other effects. There is not need for it to be Lumen specific.
void FDeferredShadingSceneRenderer::SetupLumenHardwareRayTracingHitGroupBuffer(FRDGBuilder& GraphBuilder, FViewInfo& View)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::BuildLumenHardwareRayTracingHitGroupData);

	const uint32 NumTotalSegments = FMath::Max(Scene->RayTracingSBT.GetNumGeometrySegments(), 1u);

	FRDGUploadData<Lumen::FHitGroupRootConstants> HitGroupData(GraphBuilder, NumTotalSegments);

	// if buffer is persistent then dirty bindings could be used to perform partial update
	const uint32 NumTotalShaderBindings = View.VisibleRayTracingShaderBindings.Num();

	if(NumTotalShaderBindings > 0)
	{
		const uint32 TargetBindingsPerTask = 512;

		// Distribute work evenly to the available task graph workers based on NumTotalShaderBindings.
		const uint32 NumThreads = FMath::Min(FTaskGraphInterface::Get().GetNumWorkerThreads(), CVarRHICmdWidth.GetValueOnRenderThread());
		const uint32 NumTasks = FMath::Min(NumThreads, FMath::DivideAndRoundUp(NumTotalShaderBindings, TargetBindingsPerTask));
		const uint32 NumBindingsPerTask = FMath::DivideAndRoundUp(NumTotalShaderBindings, NumTasks);

		for (uint32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
		{
			const uint32 FirstTaskBindingIndex = TaskIndex * NumBindingsPerTask;
			const FRayTracingShaderBindingData* RTShaderBindings = View.VisibleRayTracingShaderBindings.GetData() + FirstTaskBindingIndex;
			const uint32 NumBindings = FMath::Min(NumBindingsPerTask, NumTotalShaderBindings - FirstTaskBindingIndex);

			GraphBuilder.AddSetupTask([RTShaderBindings, NumBindings, HitGroupData]()
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(BuildLumenHardwareRayTracingHitGroupDataTask);

					for (uint32 BindingIndex = 0; BindingIndex < NumBindings; ++BindingIndex)
					{
						const FRayTracingShaderBindingData& RTShaderBinding = RTShaderBindings[BindingIndex];
						const FRayTracingMeshCommand& MeshCommand = *RTShaderBinding.RayTracingMeshCommand;

						// Only store hit group data for single shader slot for lightwight SBT
						// NOTE: InstanceContributionToHitGroupIndex stored in instance data is also divided by RAY_TRACING_NUM_SHADER_SLOTS in the shader
						const uint32 HitGroupIndex = RTShaderBinding.SBTRecordIndex / RAY_TRACING_NUM_SHADER_SLOTS;
						HitGroupData[HitGroupIndex].UserData = CalculateLumenHardwareRayTracingUserData(RTShaderBinding, MeshCommand);
					}
				});
		}
	}
	
	View.LumenHardwareRayTracingHitDataBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("LumenHardwareRayTracingHitDataBuffer"), HitGroupData);
}

void FDeferredShadingSceneRenderer::CreateLumenHardwareRayTracingMaterialPipeline(
	FRDGBuilder& GraphBuilder, 
	const TArrayView<FRHIRayTracingShader*>& RayGenShaderTable,
	uint32& OutMaxLocalBindingDataSize)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::CreateLumenHardwareRayTracingMaterialPipeline);
	SCOPE_CYCLE_COUNTER(STAT_CreateLumenRayTracingPipeline);

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ShaderPlatform);

	FRHICommandList& RHICmdList = GraphBuilder.RHICmdList;

	FRayTracingPipelineStateInitializer Initializer;

	const FShaderBindingLayout* ShaderBindingLayout = RayTracing::GetShaderBindingLayout(ShaderPlatform);
	if (ShaderBindingLayout)
	{
		Initializer.ShaderBindingLayout = &ShaderBindingLayout->RHILayout;
	}

	Initializer.SetRayGenShaderTable(RayGenShaderTable);

	Initializer.MaxPayloadSizeInBytes = GetRayTracingPayloadTypeMaxSize(ERayTracingPayloadType::LumenMinimal);

	// Get the ray tracing materials
	FLumenHardwareRayTracingMaterialHitGroup::FPermutationDomain PermutationVector;

	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::Disabled);
	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
	auto HitGroupShader = ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::AHS);
	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
	auto HitGroupShaderWithAvoidSelfIntersections = ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::Disabled);
	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
	auto HitGroupShaderNaniteRT = ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::AHS);
	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
	auto HitGroupShaderNaniteRTWithAvoidSelfIntersections = ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

	FRHIRayTracingShader* HitShaderTable[] = {
		HitGroupShader.GetRayTracingShader(),
		HitGroupShaderWithAvoidSelfIntersections.GetRayTracingShader(),
		HitGroupShaderNaniteRT.GetRayTracingShader(),
		HitGroupShaderNaniteRTWithAvoidSelfIntersections.GetRayTracingShader()
	};
	Initializer.SetHitGroupTable(HitShaderTable);

	auto MissShader = ShaderMap->GetShader<FLumenHardwareRayTracingMaterialMS>();
	FRHIRayTracingShader* MissShaderTable[] = { MissShader.GetRayTracingShader() };
	Initializer.SetMissShaderTable(MissShaderTable);

	OutMaxLocalBindingDataSize = FMath::Max(Initializer.GetMaxLocalBindingDataSize(), OutMaxLocalBindingDataSize);

	FRayTracingPipelineState* PipelineState = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer);

	// Send RTPSO to all views since they all share the same one
	EnumerateLinkedViews([PipelineState](FViewInfo& View)
		{
			if (View.bHasAnyRayTracingPass)
			{
				View.LumenRayTracingData.PipelineState = PipelineState;
			}
			return true;
		});
}

void FDeferredShadingSceneRenderer::SetupLumenHardwareRayTracingHitGroupBindings(FRDGBuilder& GraphBuilder, FViewInfo& View)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::SetupLumenHardwareRayTracingHitGroupBindings);

	FRHIUniformBuffer* LumenHardwareRayTracingUniformBuffer = View.LumenHardwareRayTracingUniformBuffer;

	struct FBinding
	{
		int32 ShaderIndexInPipeline;
		uint32 NumUniformBuffers;
		FRHIUniformBuffer** UniformBufferArray;
	};

	auto SetupBinding = [&](FLumenHardwareRayTracingMaterialHitGroup::FPermutationDomain PermutationVector)
		{
			auto Shader = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);
			auto HitGroupShader = Shader.GetRayTracingShader();

			FBinding Binding;
			Binding.ShaderIndexInPipeline = FindRayTracingHitGroupIndex(View.LumenRayTracingData.PipelineState, HitGroupShader, true);
			Binding.NumUniformBuffers = Shader->ParameterMapInfo.UniformBuffers.Num();
			Binding.UniformBufferArray = (FRHIUniformBuffer**)View.LumenRayTracingData.MaterialBindingsMemory.Alloc(sizeof(FRHIUniformBuffer*) * Binding.NumUniformBuffers, alignof(FRHIUniformBuffer*));

			const auto& LumenHardwareRayTracingUniformBufferParameter = Shader->GetUniformBufferParameter<FLumenHardwareRayTracingUniformBufferParameters>();
			const auto& ViewUniformBufferParameter = Shader->GetUniformBufferParameter<FViewUniformShaderParameters>();
			const auto& SceneUniformBufferParameter = Shader->GetUniformBufferParameter<FSceneUniformParameters>();
			const auto& NaniteUniformBufferParameter = Shader->GetUniformBufferParameter<FNaniteRayTracingUniformParameters>();

			if (LumenHardwareRayTracingUniformBufferParameter.IsBound())
			{
				check(LumenHardwareRayTracingUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
				Binding.UniformBufferArray[LumenHardwareRayTracingUniformBufferParameter.GetBaseIndex()] = LumenHardwareRayTracingUniformBuffer;
			}

			if (ViewUniformBufferParameter.IsBound())
			{
				check(ViewUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
				Binding.UniformBufferArray[ViewUniformBufferParameter.GetBaseIndex()] = View.ViewUniformBuffer.GetReference();
			}

			if (SceneUniformBufferParameter.IsBound())
			{
				check(SceneUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
				Binding.UniformBufferArray[SceneUniformBufferParameter.GetBaseIndex()] = GetSceneUniforms().GetBufferRHI(GraphBuilder);
			}

			if (NaniteUniformBufferParameter.IsBound())
			{
				check(NaniteUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
				Binding.UniformBufferArray[NaniteUniformBufferParameter.GetBaseIndex()] = Nanite::GRayTracingManager.GetUniformBufferRHI(GraphBuilder);
			}

			return Binding;
		};

	FBinding* ShaderBindings = (FBinding*)View.LumenRayTracingData.MaterialBindingsMemory.Alloc(sizeof(FBinding) * LumenHardwareRayTracing::NumHitGroups, alignof(FBinding));
	FBinding* ShaderBindingsNaniteRT = (FBinding*)View.LumenRayTracingData.MaterialBindingsMemory.Alloc(sizeof(FBinding) * LumenHardwareRayTracing::NumHitGroups, alignof(FBinding));

	{
		FLumenHardwareRayTracingMaterialHitGroup::FPermutationDomain PermutationVector;

		{
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::Disabled);
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
			ShaderBindings[0] = SetupBinding(PermutationVector);

			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::AHS);
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
			ShaderBindings[1] = SetupBinding(PermutationVector);
		}

		{
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::Disabled);
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
			ShaderBindingsNaniteRT[0] = SetupBinding(PermutationVector);

			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::AHS);
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
			ShaderBindingsNaniteRT[1] = SetupBinding(PermutationVector);
		}
	}

	AddRayTracingLocalShaderBindingWriterTasks(GraphBuilder, View.DirtyPersistentRayTracingShaderBindings, View.LumenRayTracingData.MaterialBindings, 
	                                           [ShaderBindings, ShaderBindingsNaniteRT](const FRayTracingShaderBindingData& RTShaderBindingData, FRayTracingLocalShaderBindingWriter* BindingWriter)
	{
		const FRayTracingMeshCommand& MeshCommand = *RTShaderBindingData.RayTracingMeshCommand;

		for (uint32 SlotIndex = 0; SlotIndex < LumenHardwareRayTracing::NumHitGroups; ++SlotIndex)
		{
			FRayTracingLocalShaderBindings& Binding = BindingWriter->AddWithExternalParameters();
			Binding.RecordIndex = RTShaderBindingData.SBTRecordIndex + SlotIndex;
			Binding.Geometry = RTShaderBindingData.RayTracingGeometry;
			Binding.SegmentIndex = MeshCommand.GeometrySegmentIndex;							
			Binding.BindingType = RTShaderBindingData.BindingType;
			Binding.UserData = CalculateLumenHardwareRayTracingUserData(RTShaderBindingData, MeshCommand);

			const FBinding& LumenBinding = MeshCommand.IsUsingNaniteRayTracing() ? ShaderBindingsNaniteRT[SlotIndex] : ShaderBindings[SlotIndex];
			Binding.ShaderIndexInPipeline = LumenBinding.ShaderIndexInPipeline;
			Binding.UniformBuffers = LumenBinding.UniformBufferArray;
			Binding.NumUniformBuffers = LumenBinding.NumUniformBuffers;
		}
	});
}

#endif // RHI_RAYTRACING