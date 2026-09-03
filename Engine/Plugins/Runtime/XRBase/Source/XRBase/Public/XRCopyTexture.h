// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"
#include "RHIFeatureLevel.h"
#include "RHIShaderPlatform.h"
#include "RHIDefinitions.h"
#include "RenderGraphDefinitions.h"

enum class EXRCopyTextureBlendModifier : uint8
{
	// Copy RGB, clear alpha to 1.0
	Opaque,

	// Copy RGBA values, overwriting target
	TransparentAlphaPassthrough,

	// Composite onto target with premultiplied alpha blend factors
	PremultipliedAlphaBlend,

	// Copy RGB and invert A, overwriting target
	InvertAlpha,
};

struct FXRCopyTextureOptions
{
	FStaticFeatureLevel FeatureLevel;
	FStaticShaderPlatform ShaderPlatform;
	ERenderTargetLoadAction LoadAction = ERenderTargetLoadAction::ELoad;
	EXRCopyTextureBlendModifier BlendMod = EXRCopyTextureBlendModifier::Opaque;
	bool bClearBlack = false;

	/**
	 * If this is set and the dst has mips, those mips will be filled in by the copy.
	 * If the source texture has mips, the copy will be repeated for each mip level of the dest texture.
	 * If the source texture does not have mips, FGenerateMips will be used on the dest texture after copying.
	 */
	bool bOutputMipChain = false;

	bool bNeedsDisplayMapping = false;
	bool bSrcIsLinear = false;
	bool bSrcSupportsHDR = false;
	bool bDstSupportsHDR = false;
	EDisplayColorGamut SrcColorGamut = EDisplayColorGamut::sRGB_D65;
	EDisplayColorGamut DstColorGamut = EDisplayColorGamut::sRGB_D65;
	EDisplayOutputFormat SrcDisplayFormat = EDisplayOutputFormat::SDR_ExplicitGammaMapping;
	EDisplayOutputFormat DstDisplayFormat = EDisplayOutputFormat::SDR_ExplicitGammaMapping;

	FXRCopyTextureOptions(FStaticFeatureLevel InFeatureLevel)
		: FeatureLevel(InFeatureLevel)
		, ShaderPlatform(GetFeatureLevelShaderPlatform(InFeatureLevel))
	{}
	FXRCopyTextureOptions(FStaticFeatureLevel InFeatureLevel, FStaticShaderPlatform InShaderPlatform)
		: FeatureLevel(InFeatureLevel)
		, ShaderPlatform(InShaderPlatform)
	{}
	void XRBASE_API SetDisplayMappingOptions(class IStereoRenderTargetManager* HDRManager);
};

void XRBASE_API AddXRCopyTexturePass(FRDGBuilder& GraphBuilder, FRDGEventName&& Name, FRDGTextureRef SrcTexture, FIntRect SrcRect, FRDGTextureRef DstTexture, FIntRect DstRect, const FXRCopyTextureOptions& Options);

UE_DEPRECATED(5.6, "This will be removed from the public API")
void XRBASE_API XRCopyTexture_InRenderPass(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FIntRect SrcRect,
	FRHITexture* DstTexture, FIntRect DstRect, const FXRCopyTextureOptions& Options, uint32 ArraySlice = 0, uint32 MipLevel = 0);
