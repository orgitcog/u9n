// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCache/MaterialCacheShaders.h"
#include "MaterialCache/MaterialCache.h"
#include "DataDrivenShaderPlatformInfo.h"

using FMaterialCacheUnwrapVS0 = FMaterialCacheUnwrapVS<false>;
using FMaterialCacheUnwrapVS1 = FMaterialCacheUnwrapVS<true>;

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FMaterialCacheUnwrapVS0, TEXT("/Engine/Private/MaterialCache/MaterialCacheUnwrapVertexShader.usf"), TEXT("Main"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FMaterialCacheUnwrapVS1, TEXT("/Engine/Private/MaterialCache/MaterialCacheUnwrapVertexShader.usf"), TEXT("Main"), SF_Vertex);

IMPLEMENT_MATERIAL_SHADER_TYPE(, FMaterialCacheUnwrapPS,      TEXT("/Engine/Private/MaterialCache/MaterialCacheUnwrapPixelShader.usf"),  TEXT("Main"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FMaterialCacheNaniteShadeCS, TEXT("/Engine/Private/MaterialCache/MaterialCacheUnwrapNaniteShade.usf"),  TEXT("Main"), SF_Compute);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FMaterialCacheShadeCS,       TEXT("/Engine/Private/MaterialCache/MaterialCacheShade.usf"),              TEXT("Main"), SF_Compute);

IMPLEMENT_GLOBAL_SHADER(FMaterialCacheABufferWritePagesCS, "/Engine/Private/MaterialCache/MaterialCacheABufferPages.usf", "WritePagesMain", SF_Compute);

template<bool bSupportsViewportFromVS>
bool FMaterialCacheUnwrapVS<bSupportsViewportFromVS>::ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	const bool bSupportsMaterialCache = Parameters.MaterialParameters.bSupportsMaterialCache || Parameters.MaterialParameters.bIsDefaultMaterial;
	return IsMaterialCacheSupported(Parameters.Platform) && bSupportsMaterialCache;
}

template<bool bSupportsViewportFromVS>
void FMaterialCacheUnwrapVS<bSupportsViewportFromVS>::ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("MATERIAL_CACHE"), 1);
	OutEnvironment.SetDefine(TEXT("SUPPORTS_VIEWPORT_FROM_VS"), bSupportsViewportFromVS);

	// TODO[MP]: Add permutation for lack of support
	OutEnvironment.CompilerFlags.Add(CFLAG_RootConstants);
}

bool FMaterialCacheUnwrapPS::ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	const bool bSupportsMaterialCache = Parameters.MaterialParameters.bSupportsMaterialCache || Parameters.MaterialParameters.bIsDefaultMaterial;
	return IsMaterialCacheSupported(Parameters.Platform) && bSupportsMaterialCache;
}

void FMaterialCacheUnwrapPS::ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
	OutEnvironment.SetDefine(TEXT("SUBSTRATE_USE_FULLYSIMPLIFIED_MATERIAL"), 1);
	OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), 1);
	OutEnvironment.SetDefine(TEXT("MATERIAL_CACHE"), 1);
}

bool FMaterialCacheNaniteShadeCS::ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	const bool bSupportsMaterialCache = Parameters.MaterialParameters.bSupportsMaterialCache || Parameters.MaterialParameters.bIsDefaultMaterial;

	return
		IsMaterialCacheSupported(Parameters.Platform) &&
		RHIGetBindlessSupport(Parameters.Platform) == ERHIBindlessSupport::AllShaderTypes &&
		// Hack: There's something awry with detecting the bindless support from SP alone
		RHIGetRuntimeBindlessResourcesConfiguration(GMaxRHIShaderPlatform) == ERHIBindlessConfiguration::AllShaders &&
		Parameters.VertexFactoryType->SupportsNaniteRendering() &&
		Parameters.VertexFactoryType->SupportsComputeShading() &&
		bSupportsMaterialCache;
}

FMaterialCacheNaniteShadeCS::FMaterialCacheNaniteShadeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer): FMeshMaterialShader(Initializer)
{
	PassDataParam.Bind(Initializer.ParameterMap, TEXT("PassData"));
	PageIndirectionsParam.Bind(Initializer.ParameterMap, TEXT("PageIndirections"));
}

void FMaterialCacheNaniteShadeCS::ModifyCompilationEnvironment(const FMeshMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
	OutEnvironment.SetDefine(TEXT("SUBSTRATE_USE_FULLYSIMPLIFIED_MATERIAL"), 1);
	OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), 1);
	OutEnvironment.SetDefine(TEXT("MATERIAL_CACHE"), 1);
	
	// Force shader model 6.0+
	OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);
	OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	OutEnvironment.CompilerFlags.Add(CFLAG_RootConstants);
	OutEnvironment.CompilerFlags.Add(CFLAG_CheckForDerivativeOps);
}

void FMaterialCacheNaniteShadeCS::SetPassParameters(FRHIBatchedShaderParameters& BatchedParameters, const FUintVector4& PassData, FRHIShaderResourceView* PageIndirections)
{
	SetShaderValue(BatchedParameters, PassDataParam, PassData);
	SetSRVParameter(BatchedParameters, PageIndirectionsParam, PageIndirections);
}

bool FMaterialCacheShadeCS::ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	const bool bSupportsMaterialCache = Parameters.MaterialParameters.bSupportsMaterialCache || Parameters.MaterialParameters.bIsDefaultMaterial;

	return
		IsMaterialCacheSupported(Parameters.Platform) &&
		RHIGetBindlessSupport(Parameters.Platform) == ERHIBindlessSupport::AllShaderTypes &&
		// Hack: There's something awry with detecting the bindless support from SP alone
		RHIGetRuntimeBindlessResourcesConfiguration(GMaxRHIShaderPlatform) == ERHIBindlessConfiguration::AllShaders &&
		Parameters.VertexFactoryType->SupportsNaniteRendering() &&
		Parameters.VertexFactoryType->SupportsComputeShading() &&
		bSupportsMaterialCache;
}

FMaterialCacheShadeCS::FMaterialCacheShadeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer): FMeshMaterialShader(Initializer)
{
	PassDataParam.Bind(Initializer.ParameterMap, TEXT("PassData"));
	PageIndirectionsParam.Bind(Initializer.ParameterMap, TEXT("PageIndirections"));
}

void FMaterialCacheShadeCS::ModifyCompilationEnvironment(const FMeshMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
	OutEnvironment.SetDefine(TEXT("SUBSTRATE_USE_FULLYSIMPLIFIED_MATERIAL"), 1);
	OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), 1);
	OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 1);
	OutEnvironment.SetDefine(TEXT("MATERIAL_CACHE"), 1);

	// Force shader model 6.0+
	OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);
	OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	OutEnvironment.CompilerFlags.Add(CFLAG_RootConstants);
	OutEnvironment.CompilerFlags.Add(CFLAG_CheckForDerivativeOps);
}

void FMaterialCacheShadeCS::SetPassParameters(FRHIBatchedShaderParameters& BatchedParameters, const FUintVector4& PassData, FRHIShaderResourceView* PageIndirections)
{
	SetShaderValue(BatchedParameters, PassDataParam, PassData);
	SetSRVParameter(BatchedParameters, PageIndirectionsParam, PageIndirections);
}

bool FMaterialCacheABufferWritePagesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsMaterialCacheSupported(Parameters.Platform);
}

/** Instantiations **/

template class FMaterialCacheUnwrapVS<false>;
template class FMaterialCacheUnwrapVS<true>;
