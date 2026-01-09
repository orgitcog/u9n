// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeTexturePatchPS.h"

#include "LandscapeUtils.h"
#include "LandscapePatchUtil.h"
#include "PixelShaderUtils.h"

namespace UE::Landscape
{

bool FApplyLandscapeTextureHeightPatchPSBase::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return DoesPlatformSupportEditLayers(Parameters.Platform);
}

void FApplyLandscapeTextureHeightPatchPSBase::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("APPLY_HEIGHT_PATCH"), 1);

	// Make our flag choices match in the shader.
	OutEnvironment.SetDefine(TEXT("RECTANGULAR_FALLOFF_FLAG"), static_cast<uint8>(EFlags::RectangularFalloff));
	OutEnvironment.SetDefine(TEXT("APPLY_PATCH_ALPHA_FLAG"), static_cast<uint8>(EFlags::ApplyPatchAlpha));
	OutEnvironment.SetDefine(TEXT("INPUT_IS_PACKED_HEIGHT_FLAG"), static_cast<uint8>(EFlags::InputIsPackedHeight));

	OutEnvironment.SetDefine(TEXT("ALPHA_BLEND_MODE"), static_cast<uint8>(ELandscapeTexturePatchBlendMode::AlphaBlend));
	OutEnvironment.SetDefine(TEXT("ADDITIVE_MODE"), static_cast<uint8>(ELandscapeTexturePatchBlendMode::Additive));
	OutEnvironment.SetDefine(TEXT("MIN_MODE"), static_cast<uint8>(ELandscapeTexturePatchBlendMode::Min));
	OutEnvironment.SetDefine(TEXT("MAX_MODE"), static_cast<uint8>(ELandscapeTexturePatchBlendMode::Max));
}

bool FOffsetHeightmapPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return DoesPlatformSupportEditLayers(Parameters.Platform);
}

void FOffsetHeightmapPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("OFFSET_HEIGHT_PATCH"), 1);
}

void FOffsetHeightmapPS::AddToRenderGraph(FRDGBuilder& GraphBuilder, FParameters* InParameters)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FOffsetHeightmapPS> PixelShader(ShaderMap);

	FIntVector TextureSize = InParameters->InHeightmap->Desc.Texture->Desc.GetSize();

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("OffsetHeightmap"),
		PixelShader,
		InParameters,
		FIntRect(0, 0, TextureSize.X, TextureSize.Y));
}

bool FSimpleTextureCopyPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return DoesPlatformSupportEditLayers(Parameters.Platform);
}

void FSimpleTextureCopyPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("SIMPLE_TEXTURE_COPY"), 1);
}

void FSimpleTextureCopyPS::AddToRenderGraph(FRDGBuilder& GraphBuilder, FRDGTextureRef SourceTexture, FRDGTextureRef DestinationTexture)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FSimpleTextureCopyPS> PixelShader(ShaderMap);

	FRDGTextureSRVRef SourceSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(SourceTexture, 0));

	FSimpleTextureCopyPS::FParameters* ShaderParams = GraphBuilder.AllocParameters<FSimpleTextureCopyPS::FParameters>();
	ShaderParams->InSource = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(SourceTexture, 0));
	ShaderParams->InSourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();
	ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);
	FIntVector DestinationSize = DestinationTexture->Desc.GetSize();
	ShaderParams->InDestinationResolution = FVector2f(DestinationSize.X, DestinationSize.Y);

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("SimpleTextureCopy"),
		PixelShader,
		ShaderParams,
		FIntRect(0, 0, DestinationSize.X, DestinationSize.Y));
}

bool FConvertToNativeLandscapePatchPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return DoesPlatformSupportEditLayers(Parameters.Platform);
}

void FConvertToNativeLandscapePatchPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("CONVERT_TO_NATIVE_LANDSCAPE_PATCH"), 1);
}

void FConvertToNativeLandscapePatchPS::AddToRenderGraph(FRDGBuilder& GraphBuilder, FRDGTextureRef SourceTexture, 
	FRDGTextureRef DestinationTexture, const FLandscapeHeightPatchConvertToNativeParams& Params)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FConvertToNativeLandscapePatchPS> PixelShader(ShaderMap);

	FRDGTextureSRVRef SourceSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(SourceTexture, 0));

	FConvertToNativeLandscapePatchPS::FParameters* ShaderParams = GraphBuilder.AllocParameters<FConvertToNativeLandscapePatchPS::FParameters>();
	ShaderParams->InHeightmap = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(SourceTexture, 0));
	ShaderParams->InZeroInEncoding = Params.ZeroInEncoding;
	ShaderParams->InHeightScale = Params.HeightScale;
	ShaderParams->InHeightOffset = Params.HeightOffset;
	ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);
	FIntVector DestinationSize = DestinationTexture->Desc.GetSize();

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("ConvertToNativeLandscapePatch"),
		PixelShader,
		ShaderParams,
		FIntRect(0, 0, DestinationSize.X, DestinationSize.Y));
}

bool FConvertBackFromNativeLandscapePatchPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return DoesPlatformSupportEditLayers(Parameters.Platform);
}

void FConvertBackFromNativeLandscapePatchPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("CONVERT_BACK_FROM_NATIVE_LANDSCAPE_PATCH"), 1);
}

void FConvertBackFromNativeLandscapePatchPS::AddToRenderGraph(FRDGBuilder& GraphBuilder, FRDGTextureRef SourceTexture,
	FRDGTextureRef DestinationTexture, const FLandscapeHeightPatchConvertToNativeParams& Params)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FConvertBackFromNativeLandscapePatchPS> PixelShader(ShaderMap);

	FRDGTextureSRVRef SourceSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(SourceTexture, 0));

	FConvertBackFromNativeLandscapePatchPS::FParameters* ShaderParams = GraphBuilder.AllocParameters<FConvertBackFromNativeLandscapePatchPS::FParameters>();
	ShaderParams->InHeightmap = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(SourceTexture, 0));
	ShaderParams->InZeroInEncoding = Params.ZeroInEncoding;
	ShaderParams->InHeightScale = Params.HeightScale;
	ShaderParams->InHeightOffset = Params.HeightOffset;
	ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);
	FIntVector DestinationSize = DestinationTexture->Desc.GetSize();

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("ConvertBackFromNativeLandscapePatch"),
		PixelShader,
		ShaderParams,
		FIntRect(0, 0, DestinationSize.X, DestinationSize.Y));
}

bool FApplyLandscapeTextureWeightPatchPSBase::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return DoesPlatformSupportEditLayers(Parameters.Platform);
}

void FApplyLandscapeTextureWeightPatchPSBase::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("APPLY_WEIGHT_PATCH"), 1);

	// Make our flag choices match in the shader.
	OutEnvironment.SetDefine(TEXT("RECTANGULAR_FALLOFF_FLAG"), static_cast<uint8>(EFlags::RectangularFalloff));
	OutEnvironment.SetDefine(TEXT("APPLY_PATCH_ALPHA_FLAG"), static_cast<uint8>(EFlags::ApplyPatchAlpha));

	OutEnvironment.SetDefine(TEXT("ALPHA_BLEND_MODE"), static_cast<uint8>(ELandscapeTexturePatchBlendMode::AlphaBlend));
	OutEnvironment.SetDefine(TEXT("ADDITIVE_MODE"), static_cast<uint8>(ELandscapeTexturePatchBlendMode::Additive));
	OutEnvironment.SetDefine(TEXT("MIN_MODE"), static_cast<uint8>(ELandscapeTexturePatchBlendMode::Min));
	OutEnvironment.SetDefine(TEXT("MAX_MODE"), static_cast<uint8>(ELandscapeTexturePatchBlendMode::Max));
}

bool FReinitializeLandscapePatchPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return DoesPlatformSupportEditLayers(Parameters.Platform);
}

void FReinitializeLandscapePatchPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("REINITIALIZE_PATCH"), 1);
}

void FReinitializeLandscapePatchPS::AddToRenderGraph(FRDGBuilder& GraphBuilder, FParameters* InParameters, bool bHeightPatch)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	FReinitializeLandscapePatchPS::FPermutationDomain PermutationDomain;
	PermutationDomain.Set<FReinitializeLandscapePatchPS::FHeightPatch>(bHeightPatch);
	TShaderMapRef<FReinitializeLandscapePatchPS> PixelShader(ShaderMap, PermutationDomain);

	FIntVector DestinationSize = InParameters->RenderTargets[0].GetTexture()->Desc.GetSize();

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("ReinitializeLandscapeTexturePatch"),
		PixelShader,
		InParameters,
		FIntRect(0, 0, DestinationSize.X, DestinationSize.Y));
}

}//end UE::Landscape

IMPLEMENT_SHADER_TYPE(template<>, UE::Landscape::FApplyLandscapeTextureHeightPatchPS</*bPerformBlending = */true>, TEXT("/Plugin/LandscapePatch/Private/LandscapeTexturePatchPS.usf"), TEXT("ApplyLandscapeTextureHeightPatch"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, UE::Landscape::FApplyLandscapeTextureHeightPatchPS</*bPerformBlending = */false>, TEXT("/Plugin/LandscapePatch/Private/LandscapeTexturePatchPS.usf"), TEXT("ApplyLandscapeTextureHeightPatch"), SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(UE::Landscape::FOffsetHeightmapPS, "/Plugin/LandscapePatch/Private/LandscapeTexturePatchPS.usf", "ApplyOffsetToHeightmap", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(UE::Landscape::FSimpleTextureCopyPS, "/Plugin/LandscapePatch/Private/LandscapeTexturePatchPS.usf", "SimpleTextureCopy", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(UE::Landscape::FConvertToNativeLandscapePatchPS, "/Plugin/LandscapePatch/Private/LandscapeTexturePatchPS.usf", "ConvertToNativeLandscapePatch", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(UE::Landscape::FConvertBackFromNativeLandscapePatchPS, "/Plugin/LandscapePatch/Private/LandscapeTexturePatchPS.usf", "ConvertBackFromNativeLandscapePatch", SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, UE::Landscape::FApplyLandscapeTextureWeightPatchPS</*bPerformBlending = */true>, TEXT("/Plugin/LandscapePatch/Private/LandscapeTexturePatchPS.usf"), TEXT("ApplyLandscapeTextureWeightPatch"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, UE::Landscape::FApplyLandscapeTextureWeightPatchPS</*bPerformBlending = */false>, TEXT("/Plugin/LandscapePatch/Private/LandscapeTexturePatchPS.usf"), TEXT("ApplyLandscapeTextureWeightPatch"), SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(UE::Landscape::FReinitializeLandscapePatchPS, "/Plugin/LandscapePatch/Private/LandscapeTexturePatchPS.usf", "ReinitializePatch", SF_Pixel);

