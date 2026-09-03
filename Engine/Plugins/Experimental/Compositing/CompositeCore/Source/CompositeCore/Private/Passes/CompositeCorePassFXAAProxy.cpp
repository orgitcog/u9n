// Copyright Epic Games, Inc. All Rights Reserved.

#include "Passes/CompositeCorePassFXAAProxy.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessAA.h"
#include "PostProcess/PostProcessMaterialInputs.h"

DECLARE_GPU_STAT_NAMED(FCompositeCoreDisplayTransform, TEXT("CompositeCore.DisplayTransform"));

class FCompositeCoreDisplayTransformShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompositeCoreDisplayTransformShader);
	SHADER_USE_PARAMETER_STRUCT(FCompositeCoreDisplayTransformShader, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(FIntPoint, Dimensions)
		SHADER_PARAMETER(uint32, bIsForward)
		SHADER_PARAMETER(float, Gamma)
		SHADER_PARAMETER(float, InvGamma)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCompositeCoreDisplayTransformShader, "/Plugin/CompositeCore/Private/CompositeCoreDisplayTransform.usf", "MainPS", SF_Pixel);

namespace UE
{
	namespace CompositeCore
	{
		namespace Private
		{
			FScreenPassTexture AddDisplayTransformPass(FRDGBuilder& GraphBuilder, const FScreenPassViewInfo ViewInfo, const FScreenPassTexture& Input, bool bIsForward, float Gamma = 2.2f)
			{
				RDG_EVENT_SCOPE_STAT(GraphBuilder, FCompositeCoreDisplayTransform, "CompositeCore.DisplayTransform");
				RDG_GPU_STAT_SCOPE(GraphBuilder, FCompositeCoreDisplayTransform);

				FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(ViewInfo.FeatureLevel);
				FScreenPassRenderTarget Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, Input, ERenderTargetLoadAction::ENoAction, TEXT("FXAACompositePass"));
				FScreenPassTextureViewport Viewport = FScreenPassTextureViewport(Output);

				FCompositeCoreDisplayTransformShader::FParameters* Parameters = GraphBuilder.AllocParameters<FCompositeCoreDisplayTransformShader::FParameters>();
				Parameters->InputTexture = Input.Texture;
				Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
				Parameters->bIsForward = bIsForward;
				Parameters->Gamma = Gamma;
				Parameters->InvGamma = 1.0f / Gamma;
				Parameters->bIsForward = bIsForward;
				Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

				TShaderMapRef<FCompositeCoreDisplayTransformShader> PixelShader(GlobalShaderMap);
				AddDrawScreenPass(
					GraphBuilder,
					RDG_EVENT_NAME("Composite.DisplayTransform (%dx%d)", Viewport.Extent.X, Viewport.Extent.Y),
					ViewInfo,
					Viewport,
					Viewport,
					PixelShader,
					Parameters
				);

				return Output;
			}
		}

		FPassOutput FFXAAPassProxy::Add(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPassInputArray& Inputs, const FPassContext& PassContext) const
		{
			FScreenPassTexture Input = Inputs[0].Texture;
			FResourceMetadata Metadata = Inputs[0].Metadata;
			const bool bLinearSourceColors = (Metadata.Encoding == EEncoding::Linear);

			if (bLinearSourceColors)
			{
				// We tonemap & encode the result so that FXAA can operate on perceptual colors
				constexpr bool bIsForward = true;
				Input = Private::AddDisplayTransformPass(GraphBuilder, InView, Input, bIsForward);
			}

			static IConsoleVariable* FXAAQualityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FXAA.Quality"));
			check(FXAAQualityCVar);
			const int32 Quality = QualityOverride.IsSet() ? QualityOverride.GetValue() : FXAAQualityCVar->GetInt();

			FFXAAInputs FXAAInputs{};
			FXAAInputs.SceneColor = Input;
			FXAAInputs.Quality = static_cast<EFXAAQuality>(FMath::Clamp(Quality, 0, static_cast<int32>(EFXAAQuality::MAX) - 1));
			FXAAInputs.OverrideOutput = Inputs.OverrideOutput;

			FScreenPassTexture Output = AddFXAAPass(GraphBuilder, InView, FXAAInputs);

			if (bLinearSourceColors)
			{
				// We decode and invert the tonemapping to obtain linear colors again.
				constexpr bool bIsForward = false;
				Output = Private::AddDisplayTransformPass(GraphBuilder, InView, Output, bIsForward);
			}

			return FPassOutput{ MoveTemp(Output), Metadata, PassOutputOverride };
		}
	}
}

