// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"
#include "GlobalShader.h"
#include "Lumen/LumenTracingUtils.h"
#include "RayTracing/RayTracingLighting.h"
#include "RayTracing/RayTracing.h"
#include "RayTracingPayloadType.h"
#include "SceneTextureParameters.h"
#include "Substrate/Substrate.h" 

enum class EReflectionsMethod;

namespace RayTracing
{
	struct FSceneOptions;
};

namespace LumenHardwareRayTracing
{
	enum class EAvoidSelfIntersectionsMode : uint8
	{
		Disabled,
		Retrace,
		AHS,

		MAX
	};
	
	enum class EHitLightingMode
	{
		SurfaceCache,
		HitLighting,
		HitLightingForReflections,

		MAX
	};

	bool IsInlineSupported();
	bool IsRayGenSupported();
	float GetFarFieldBias();
	bool UseSurfaceCacheAlphaMasking();	
	EAvoidSelfIntersectionsMode GetAvoidSelfIntersectionsMode();

	// Hit Lighting
	EHitLightingMode GetHitLightingMode(const FViewInfo& View, EDiffuseIndirectMethod DiffuseIndirectMethod);
	uint32 GetHitLightingShadowMode();
	uint32 GetHitLightingShadowTranslucencyMode();
	bool UseHitLightingForceOpaque();
	bool UseHitLightingDirectLighting();
	bool UseHitLightingSkylight(EDiffuseIndirectMethod DiffuseIndirectMethod);
	bool UseReflectionCapturesForHitLighting();

	bool UseShaderExecutionReordering();

	void SetRayTracingSceneOptions(const FViewInfo& View, EDiffuseIndirectMethod DiffuseIndirectMethod, EReflectionsMethod ReflectionsMethod, RayTracing::FSceneOptions& SceneOptions);
}

#if RHI_RAYTRACING

namespace Lumen
{
	// Struct definitions much match those in LumenHardwareRayTracingCommon.ush 
	struct FHitGroupRootConstants
	{
		uint32 UserData;
	};

	enum class ERayTracingShaderDispatchType
	{
		RayGen = 0,
		Inline = 1
	};
}

class FLumenHardwareRayTracingShaderBase : public FGlobalShader
{
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FSharedParameters, )
		// Scene includes
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, FarFieldTLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, RayTracingSceneMetadata)

		// Ray tracing feedback buffer
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWInstanceHitCountBuffer)

		// Nanite Ray Tracing
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRayTracingUniformParameters, NaniteRayTracing)

		// Lighting structures
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FRayTracingLightGrid, LightGridParameters)
		SHADER_PARAMETER_STRUCT_REF(FReflectionCaptureShaderData, ReflectionCapture)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightUniformParameters, ForwardLightStruct)

		// Lumen
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER(uint32, MeshSectionVisibilityTest)
		SHADER_PARAMETER(float, MinTraceDistanceToSampleSurfaceCache)
		SHADER_PARAMETER(float, SurfaceCacheSamplingDepthBias)

		// Inline data
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<Lumen::FHitGroupRootConstants>, HitGroupData)
		SHADER_PARAMETER_STRUCT_REF(FLumenHardwareRayTracingUniformBufferParameters, LumenHardwareRayTracingUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	FLumenHardwareRayTracingShaderBase();
	FLumenHardwareRayTracingShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	
	class FUseThreadGroupSize64 : SHADER_PERMUTATION_BOOL("RAY_TRACING_USE_THREAD_GROUP_SIZE_64");
	class FUseTracingFeedback : SHADER_PERMUTATION_BOOL("ENABLE_TRACING_FEEDBACK");
	class FNaniteRayTracing : SHADER_PERMUTATION_BOOL("NANITE_RAY_TRACING");
	using FBasePermutationDomain = TShaderPermutationDomain<FUseThreadGroupSize64, FUseTracingFeedback, FNaniteRayTracing>;
	using FPermutationDomain = TShaderPermutationDomain<FBasePermutationDomain>; // The default that is used if derived classes don't define their own

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ESurfaceCacheSampling SurfaceCacheSampling, FShaderCompilerEnvironment& OutEnvironment);

	static void ModifyCompilationEnvironmentInternal(Lumen::ERayTracingShaderDispatchType ShaderDispatchType, bool UseThreadGroupSize64, FShaderCompilerEnvironment& OutEnvironment);

	static FIntPoint GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType ShaderDispatchType, bool UseThreadGroupSize64);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType);

	static bool UseThreadGroupSize64(EShaderPlatform ShaderPlatform);
};


#define DECLARE_LUMEN_RAYTRACING_SHADER(ShaderClass) \
	public: \
	ShaderClass() = default; \
	ShaderClass(const ShaderMetaType::CompiledShaderInitializerType & Initializer)\
		: FLumenHardwareRayTracingShaderBase(Initializer) {}\
	using TComputeShaderType = class ShaderClass##CS; \
	using TRayGenShaderType = class ShaderClass##RGS;

#define DECLARE_STOCHASTIC_LIGHTING_COMPUTE_RAYTRACING_SHADER(ShaderClass) \
		DECLARE_GLOBAL_SHADER(ShaderClass##CS) \
		SHADER_USE_PARAMETER_STRUCT(ShaderClass##CS, ShaderClass) \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) \
		{ \
			FPermutationDomain PermutationVector(Parameters.PermutationId); \
			if (PermutationVector.Get<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain>().Get<FUseThreadGroupSize64>() && !RHISupportsWaveSize64(Parameters.Platform)) \
			{ \
				return false; \
			} \
			return ShaderClass::ShouldCompilePermutation(Parameters, Lumen::ERayTracingShaderDispatchType::Inline); \
		}\
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)\
		{ \
			FPermutationDomain PermutationVector(Parameters.PermutationId); \
			const bool UseThreadGroupSize64 = PermutationVector.Get<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain>().Get<FUseThreadGroupSize64>() ; \
			FIntPoint Size = GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType::Inline, UseThreadGroupSize64); \
			OutEnvironment.SetDefine(TEXT("INLINE_RAY_TRACING_THREAD_GROUP_SIZE_X"), Size.X); \
			OutEnvironment.SetDefine(TEXT("INLINE_RAY_TRACING_THREAD_GROUP_SIZE_Y"), Size.Y); \
			const bool bUseTracingFeedback = PermutationVector.Get<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain>().Get<FUseTracingFeedback>(); \
			OutEnvironment.SetDefine(TEXT("ENABLE_TRACING_FEEDBACK"), bUseTracingFeedback); \
			ShaderClass::ModifyCompilationEnvironment(Parameters, Lumen::ERayTracingShaderDispatchType::Inline, OutEnvironment); \
			ModifyCompilationEnvironmentInternal(Lumen::ERayTracingShaderDispatchType::Inline, UseThreadGroupSize64, OutEnvironment); \
		}\
		static FPermutationDomain MakePermutationVector(ShaderClass::FPermutationDomain PermutationVector, const FViewInfo& View) \
		{ \
			FLumenHardwareRayTracingShaderBase::FBasePermutationDomain Base; \
			Base.Set<FUseThreadGroupSize64>(UseThreadGroupSize64(View.GetShaderPlatform())); \
			Base.Set<FUseTracingFeedback>(View.bRayTracingFeedbackEnabled); \
			Base.Set<FNaniteRayTracing>(Nanite::GetRayTracingMode() != Nanite::ERayTracingMode::Fallback); \
			PermutationVector.Set<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain>(Base); \
			return PermutationVector; \
		} \
		static FIntPoint GetThreadGroupSize(EShaderPlatform ShaderPlatform) { return GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType::Inline, UseThreadGroupSize64(ShaderPlatform)); } \
		static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId) { return static_cast<ERayTracingPayloadType>(0); } 

#define IMPLEMENT_LUMEN_COMPUTE_RAYTRACING_SHADER(ShaderClass) \
	class ShaderClass##CS : public ShaderClass \
	{ \
		DECLARE_STOCHASTIC_LIGHTING_COMPUTE_RAYTRACING_SHADER(ShaderClass) \
		static void AddLumenRayTracingDispatchIndirect(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FRDGBufferRef IndirectArgsBuffer, uint32 IndirectArgsOffset, ERDGPassFlags ComputePassFlags) \
		{ \
			TShaderRef<ShaderClass##CS> ComputeShader = View.ShaderMap->GetShader<ShaderClass##CS>(MakePermutationVector(PermutationVector, View)); \
			FComputeShaderUtils::AddPass(GraphBuilder, std::move(EventName), ComputePassFlags, ComputeShader, PassParameters, IndirectArgsBuffer, IndirectArgsOffset); \
		} \
		static void AddLumenRayTracingDispatch(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FIntVector GroupCount, ERDGPassFlags ComputePassFlags) \
		{ \
			TShaderRef<ShaderClass##CS> ComputeShader = View.ShaderMap->GetShader<ShaderClass##CS>(MakePermutationVector(PermutationVector, View)); \
			FComputeShaderUtils::AddPass(GraphBuilder, std::move(EventName), ComputePassFlags, ComputeShader, PassParameters, GroupCount); \
		} \
	};

#define IMPLEMENT_MEGALIGHT_COMPUTE_RAYTRACING_SHADER(ShaderClass) \
	class ShaderClass##CS : public ShaderClass \
	{ \
		DECLARE_STOCHASTIC_LIGHTING_COMPUTE_RAYTRACING_SHADER(ShaderClass) \
		static void AddMegaLightRayTracingDispatchIndirect(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FRDGBufferRef IndirectArgsBuffer, uint32 IndirectArgsOffset, ERDGPassFlags ComputePassFlags) \
		{ \
			TShaderRef<ShaderClass##CS> ComputeShader = View.ShaderMap->GetShader<ShaderClass##CS>(MakePermutationVector(PermutationVector, View)); \
			FComputeShaderUtils::AddPass(GraphBuilder, std::move(EventName), ComputePassFlags, ComputeShader, PassParameters, IndirectArgsBuffer, IndirectArgsOffset); \
		} \
		static void AddMegaLightRayTracingDispatch(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FIntVector GroupCount, ERDGPassFlags ComputePassFlags) \
		{ \
			TShaderRef<ShaderClass##CS> ComputeShader = View.ShaderMap->GetShader<ShaderClass##CS>(MakePermutationVector(PermutationVector, View)); \
			FComputeShaderUtils::AddPass(GraphBuilder, std::move(EventName), ComputePassFlags, ComputeShader, PassParameters, GroupCount); \
		} \
	};

// Pass helpers
template<typename TShaderClass>
static void AddStochasticLightingRayTraceDispatchPass(
	FRDGBuilder& GraphBuilder,
	FRDGEventName&& PassName,
	const TShaderRef<TShaderClass>& RayGenerationShader,
	typename TShaderClass::FParameters* Parameters,
	TRDGUniformBufferBinding<FSceneUniformParameters>& SceneUniformBuffer,
	TRDGUniformBufferBinding<FNaniteRayTracingUniformParameters>& NaniteRayTracingUniformBuffer,
	FIntPoint Resolution,
	const FViewInfo& View,
	bool bUseMinimalPayload, 
	ERDGPassFlags ComputePassFlags)
{
	ClearUnusedGraphResources(RayGenerationShader, Parameters);

	GraphBuilder.AddPass(
		Forward<FRDGEventName>(PassName),
		Parameters,
		ComputePassFlags,
		[Parameters, SceneUniformBuffer, NaniteRayTracingUniformBuffer, &View, RayGenerationShader, bUseMinimalPayload, Resolution](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			FRHIBatchedShaderParameters& GlobalResources = RHICmdList.GetScratchShaderParameters();
			SetShaderParameters(GlobalResources, RayGenerationShader, *Parameters);

			TOptional<FScopedUniformBufferStaticBindings> StaticUniformBufferScope = RayTracing::BindStaticUniformBufferBindings(View, SceneUniformBuffer->GetRHI(), NaniteRayTracingUniformBuffer->GetRHI(), RHICmdList);

			FRayTracingPipelineState* Pipeline = View.MaterialRayTracingData.PipelineState;
			FRHIShaderBindingTable* SBT = View.MaterialRayTracingData.ShaderBindingTable;

			if (bUseMinimalPayload)
			{
				Pipeline = View.LumenRayTracingData.PipelineState;
				SBT = View.LumenRayTracingData.ShaderBindingTable;
			}

			RHICmdList.RayTraceDispatch(Pipeline, RayGenerationShader.GetRayTracingShader(), SBT, GlobalResources,
				Resolution.X, Resolution.Y);
		}
	);
}

template<typename TShaderClass>
static void AddStochasticLightingRayTraceDispatchIndirectPass(
	FRDGBuilder& GraphBuilder,
	FRDGEventName&& PassName,
	const TShaderRef<TShaderClass>& RayGenerationShader,
	typename TShaderClass::FParameters* Parameters,
	TRDGUniformBufferBinding<FSceneUniformParameters>& SceneUniformBuffer,
	TRDGUniformBufferBinding<FNaniteRayTracingUniformParameters>& NaniteRayTracingUniformBuffer,
	FRDGBufferRef IndirectArgsBuffer,
	uint32 IndirectArgsOffset,
	const FViewInfo& View,
	bool bUseMinimalPayload,
	ERDGPassFlags ComputePassFlags
	)
{
	ClearUnusedGraphResources(RayGenerationShader, Parameters, { IndirectArgsBuffer });
	
	GraphBuilder.AddPass(
		Forward<FRDGEventName>(PassName),
		Parameters,
		ComputePassFlags,
		[Parameters, SceneUniformBuffer, NaniteRayTracingUniformBuffer, &View, RayGenerationShader, bUseMinimalPayload, IndirectArgsBuffer, IndirectArgsOffset](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			IndirectArgsBuffer->MarkResourceAsUsed();

			FRHIBatchedShaderParameters& GlobalResources = RHICmdList.GetScratchShaderParameters();
			SetShaderParameters(GlobalResources, RayGenerationShader, *Parameters);

			TOptional<FScopedUniformBufferStaticBindings> StaticUniformBufferScope = RayTracing::BindStaticUniformBufferBindings(View, SceneUniformBuffer->GetRHI(), NaniteRayTracingUniformBuffer->GetRHI(), RHICmdList);
			
			FRayTracingPipelineState* Pipeline = View.MaterialRayTracingData.PipelineState;
			FRHIShaderBindingTable* SBT = View.MaterialRayTracingData.ShaderBindingTable;

			if (bUseMinimalPayload)
			{
				Pipeline = View.LumenRayTracingData.PipelineState;
				SBT = View.LumenRayTracingData.ShaderBindingTable;
			}

			RHICmdList.RayTraceDispatchIndirect(Pipeline, RayGenerationShader.GetRayTracingShader(), SBT, GlobalResources,
				IndirectArgsBuffer->GetIndirectRHICallBuffer(), IndirectArgsOffset);
		}
	);
}

#define DECLARE_STOCHASTIC_LIGHTING_RAYGEN_RAYTRACING_SHADER(ShaderClass) \
		DECLARE_GLOBAL_SHADER(ShaderClass##RGS) \
		SHADER_USE_ROOT_PARAMETER_STRUCT(ShaderClass##RGS, ShaderClass) \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) \
		{ \
			FPermutationDomain PermutationVector(Parameters.PermutationId); \
			if (PermutationVector.Get<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain>().Get<FUseThreadGroupSize64>()) \
			{ \
				return false; /* Wave 64 is only relevant for CS */ \
			} \
			return ShaderClass::ShouldCompilePermutation(Parameters, Lumen::ERayTracingShaderDispatchType::RayGen); \
		} \
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment) \
		{ \
			ShaderClass::ModifyCompilationEnvironment(Parameters, Lumen::ERayTracingShaderDispatchType::RayGen, OutEnvironment); \
			ModifyCompilationEnvironmentInternal(Lumen::ERayTracingShaderDispatchType::RayGen, false, OutEnvironment); \
		} \
		static FIntPoint GetThreadGroupSize() { return GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType::RayGen, false); } \
		static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Parameters) \
		{ \
			return RayTracing::GetShaderBindingLayout(Parameters.Platform); \
		} 

#define IMPLEMENT_LUMEN_RAYGEN_RAYTRACING_SHADER(ShaderClass) \
	class ShaderClass##RGS : public ShaderClass \
	{ \
		DECLARE_STOCHASTIC_LIGHTING_RAYGEN_RAYTRACING_SHADER(ShaderClass) \
		static void AddLumenRayTracingDispatchIndirect(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FRDGBufferRef IndirectArgsBuffer, uint32 IndirectArgsOffset, bool bUseMinimalPayload, ERDGPassFlags ComputePassFlags) \
		{ \
			TShaderRef<ShaderClass##RGS> RayGenerationShader = View.ShaderMap->GetShader<ShaderClass##RGS>(PermutationVector); \
			AddStochasticLightingRayTraceDispatchIndirectPass(GraphBuilder, std::move(EventName), RayGenerationShader, PassParameters, PassParameters->SharedParameters.TracingParameters.Scene, PassParameters->SharedParameters.NaniteRayTracing, IndirectArgsBuffer, IndirectArgsOffset, View, bUseMinimalPayload, ComputePassFlags); \
		} \
		static void AddLumenRayTracingDispatch(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FIntPoint DispatchResolution, bool bUseMinimalPayload, ERDGPassFlags ComputePassFlags) \
		{ \
			TShaderRef<ShaderClass##RGS> RayGenerationShader = View.ShaderMap->GetShader<ShaderClass##RGS>(PermutationVector); \
			AddStochasticLightingRayTraceDispatchPass(GraphBuilder, std::move(EventName), RayGenerationShader, PassParameters, PassParameters->SharedParameters.TracingParameters.Scene, PassParameters->SharedParameters.NaniteRayTracing, DispatchResolution, View, bUseMinimalPayload, ComputePassFlags); \
		} \
	};
	
#define IMPLEMENT_MEGALIGHT_RAYGEN_RAYTRACING_SHADER(ShaderClass) \
	class ShaderClass##RGS : public ShaderClass \
	{ \
		DECLARE_STOCHASTIC_LIGHTING_RAYGEN_RAYTRACING_SHADER(ShaderClass) \
		static void AddMegaLightRayTracingDispatchIndirect(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FRDGBufferRef IndirectArgsBuffer, uint32 IndirectArgsOffset, bool bUseMinimalPayload, ERDGPassFlags ComputePassFlags) \
		{ \
			TShaderRef<ShaderClass##RGS> RayGenerationShader = View.ShaderMap->GetShader<ShaderClass##RGS>(PermutationVector); \
			AddStochasticLightingRayTraceDispatchIndirectPass(GraphBuilder, std::move(EventName), RayGenerationShader, PassParameters, PassParameters->MegaLightsParameters.Scene, PassParameters->NaniteRayTracing, IndirectArgsBuffer, IndirectArgsOffset, View, bUseMinimalPayload, ComputePassFlags); \
		} \
		static void AddMegaLightRayTracingDispatch(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FIntPoint DispatchResolution, bool bUseMinimalPayload, ERDGPassFlags ComputePassFlags) \
		{ \
			TShaderRef<ShaderClass##RGS> RayGenerationShader = View.ShaderMap->GetShader<ShaderClass##RGS>(PermutationVector); \
			AddStochasticLightingRayTraceDispatchPass(GraphBuilder, std::move(EventName), RayGenerationShader, PassParameters, PassParameters->MegaLightsParameters.Scene, PassParameters->NaniteRayTracing, DispatchResolution, View, bUseMinimalPayload, ComputePassFlags); \
		} \
	};	
	
#define IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(ShaderClass) \
	IMPLEMENT_LUMEN_COMPUTE_RAYTRACING_SHADER(ShaderClass) \
	IMPLEMENT_LUMEN_RAYGEN_RAYTRACING_SHADER(ShaderClass)
	
#define IMPLEMENT_MEGALIGHT_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(ShaderClass) \
	IMPLEMENT_MEGALIGHT_COMPUTE_RAYTRACING_SHADER(ShaderClass) \
	IMPLEMENT_MEGALIGHT_RAYGEN_RAYTRACING_SHADER(ShaderClass)

class FLumenHardwareRayTracingDeferredMaterialRGS : public FLumenHardwareRayTracingShaderBase
{
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FDeferredMaterialParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER(int, TileSize)
		SHADER_PARAMETER(FIntPoint, DeferredMaterialBufferResolution)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FDeferredMaterialPayload>, RWDeferredMaterialBuffer)
	END_SHADER_PARAMETER_STRUCT()

	FLumenHardwareRayTracingDeferredMaterialRGS() = default;
	FLumenHardwareRayTracingDeferredMaterialRGS(const ShaderMetaType::CompiledShaderInitializerType & Initializer)
		: FLumenHardwareRayTracingShaderBase(Initializer)
	{}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ESurfaceCacheSampling SurfaceCacheSampling, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, SurfaceCacheSampling, OutEnvironment);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform)
			&& FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(Parameters, ShaderDispatchType);
	}
};

void SetLumenHardwareRayTracingSharedParameters(
	FRDGBuilder& GraphBuilder,
	const FSceneTextureParameters& SceneTextures,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	FLumenHardwareRayTracingShaderBase::FSharedParameters* SharedParameters);

#endif // RHI_RAYTRACING

BEGIN_UNIFORM_BUFFER_STRUCT(FLumenHardwareRayTracingUniformBufferParameters, )
	SHADER_PARAMETER(float, SkipBackFaceHitDistance)
	SHADER_PARAMETER(float, SkipTwoSidedHitDistance)
	SHADER_PARAMETER(float, SkipTranslucent)
	SHADER_PARAMETER(float, DiffuseColorBoost)
END_UNIFORM_BUFFER_STRUCT()