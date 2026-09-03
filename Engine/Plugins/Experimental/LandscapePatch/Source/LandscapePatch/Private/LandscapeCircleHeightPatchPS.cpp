// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeCircleHeightPatchPS.h"

#include "PixelShaderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"

bool FLandscapeCircleHeightPatchPSBase::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	// Apparently landscape requires a particular feature level
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
		&& !IsConsolePlatform(Parameters.Platform)
		&& !IsMetalMobilePlatform(Parameters.Platform);
}

template <bool bPerformBlending>
void FLandscapeCircleHeightPatchPS<bPerformBlending>::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	if (bPerformBlending)
	{
		OutEnvironment.SetDefine(TEXT("PERFORM_BLENDING"), 1);
	}
	OutEnvironment.SetDefine(TEXT("CIRCLE_HEIGHT_PATCH"), 1);
}

template <bool bPerformBlending>
void FLandscapeCircleHeightPatchPS<bPerformBlending>::AddToRenderGraph(FRDGEventName&& InRDGEventName, FRDGBuilder& GraphBuilder, FLandscapeCircleHeightPatchPSBase::FParameters* Parameters, const FIntRect& DestinationBounds)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FLandscapeCircleHeightPatchPS> PixelShader(ShaderMap);

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		MoveTemp(InRDGEventName),
		PixelShader,
		Parameters,
		DestinationBounds);
}

template <bool bPerformBlending>
void FLandscapeCircleVisibilityPatchPS<bPerformBlending>::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	if (bPerformBlending)
	{
		OutEnvironment.SetDefine(TEXT("PERFORM_BLENDING"), 1);
	}
	OutEnvironment.SetDefine(TEXT("CIRCLE_VISIBILITY_PATCH"), 1);
}

template <bool bPerformBlending>
void FLandscapeCircleVisibilityPatchPS<bPerformBlending>::AddToRenderGraph(FRDGEventName&& InRDGEventName, FRDGBuilder& GraphBuilder, FLandscapeCircleHeightPatchPSBase::FParameters* Parameters, const FIntRect& DestinationBounds)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FLandscapeCircleVisibilityPatchPS> PixelShader(ShaderMap);

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		MoveTemp(InRDGEventName),
		PixelShader,
		Parameters,
		DestinationBounds);
}

IMPLEMENT_SHADER_TYPE(template<>, FLandscapeCircleHeightPatchPS</*bPerformBlending = */false>, TEXT("/Plugin/LandscapePatch/Private/LandscapeCircleHeightPatchPS.usf"), TEXT("ApplyLandscapeCircleHeightPatch"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FLandscapeCircleHeightPatchPS</*bPerformBlending = */true>, TEXT("/Plugin/LandscapePatch/Private/LandscapeCircleHeightPatchPS.usf"), TEXT("ApplyLandscapeCircleHeightPatch"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FLandscapeCircleVisibilityPatchPS</*bPerformBlending = */false>, TEXT("/Plugin/LandscapePatch/Private/LandscapeCircleHeightPatchPS.usf"), TEXT("ApplyLandscapeCircleVisibilityPatch"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FLandscapeCircleVisibilityPatchPS</*bPerformBlending = */true>, TEXT("/Plugin/LandscapePatch/Private/LandscapeCircleHeightPatchPS.usf"), TEXT("ApplyLandscapeCircleVisibilityPatch"), SF_Pixel);

// Explicit instantiation otherwise non-unity builds cannot link
template class FLandscapeCircleHeightPatchPS</*bPerformBlending = */false>;
template class FLandscapeCircleHeightPatchPS</*bPerformBlending = */true>;
template class FLandscapeCircleVisibilityPatchPS</*bPerformBlending = */false>;
template class FLandscapeCircleVisibilityPatchPS</*bPerformBlending = */true>;