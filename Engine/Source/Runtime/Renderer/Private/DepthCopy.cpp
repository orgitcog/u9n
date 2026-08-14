// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DepthCopy.cpp: Depth rendering implementation.
=============================================================================*/

#include "DepthCopy.h"
#include "ScenePrivate.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "PixelShaderUtils.h"

namespace DepthCopy
{

class FViewDepthCopyCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FViewDepthCopyCS)
		SHADER_USE_PARAMETER_STRUCT(FViewDepthCopyCS, FGlobalShader)

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWDepthTexture)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		END_SHADER_PARAMETER_STRUCT()

		using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static int32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FViewDepthCopyCS, "/Engine/Private/CopyDepthTextureCS.usf", "CopyDepthCS", SF_Compute);

void AddViewDepthCopyCSPass(FRDGBuilder& GraphBuilder, FViewInfo& View, FRDGTextureRef SourceSceneDepthTexture, FRDGTextureRef DestinationDepthTexture)
{
	FViewDepthCopyCS::FPermutationDomain PermutationVector;
	auto ComputeShader = View.ShaderMap->GetShader<FViewDepthCopyCS>(PermutationVector);

	FViewDepthCopyCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FViewDepthCopyCS::FParameters>();
	PassParameters->RWDepthTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DestinationDepthTexture));
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->SceneDepthTexture = SourceSceneDepthTexture;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("CopyViewDepthCS"),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FViewDepthCopyCS::GetGroupSize()));
}

IMPLEMENT_GLOBAL_SHADER(FCopyDepthPS, "/Engine/Private/CopyDepthTexture.usf", "CopyDepthPS", SF_Pixel);

void AddViewDepthCopyPSPass(FRDGBuilder& GraphBuilder, FViewInfo& View, FRDGTextureRef SourceSceneDepthTexture, FRDGTextureRef DestinationDepthTexture)
{
	const FRDGTextureDesc& SrcDesc = SourceSceneDepthTexture->Desc;

	FCopyDepthPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCopyDepthPS::FParameters>();
	if (SrcDesc.NumSamples > 1)
	{
		PassParameters->DepthTextureMS = SourceSceneDepthTexture;
	}
	else
	{
		PassParameters->DepthTexture = SourceSceneDepthTexture;
	}
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(DestinationDepthTexture, ERenderTargetLoadAction::ENoAction, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);

	FCopyDepthPS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FCopyDepthPS::FMSAASampleCount>(SrcDesc.NumSamples);
	TShaderMapRef<FCopyDepthPS> PixelShader(View.ShaderMap, PermutationVector);

	// Set depth test to always pass and stencil test to reset to 0.
	FRHIDepthStencilState* DepthStencilState = TStaticDepthStencilState<
		true, CF_Always,										// depth
		true, CF_Always, SO_Zero, SO_Zero, SO_Zero,				// frontface stencil
		true, CF_Always, SO_Zero, SO_Zero, SO_Zero				// backface stencil
	>::GetRHI();

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		View.ShaderMap,
		RDG_EVENT_NAME("CopyViewDepthPS"),
		PixelShader,
		PassParameters,
		View.ViewRect,
		nullptr, /*BlendState*/
		nullptr, /*RasterizerState*/
		DepthStencilState,
		0 /*StencilRef*/);

	// The above copy technique loses HTILE data during the copy, so until AddCopyTexturePass() supports depth buffer copies on all platforms,
	// This is the best we can do: regenerate HTile from depth texture.
	AddResummarizeHTilePass(GraphBuilder, DestinationDepthTexture);
}

};
