// Copyright Epic Games, Inc. All Rights Reserved.

#include "Passes/CompositeCorePassProxy.h"

#include "Passes/CompositeCorePassMergeProxy.h"

#include "FXRenderingUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "PostProcess/LensDistortion.h"
#include "PostProcess/PostProcessMaterialInputs.h"

static TAutoConsoleVariable<int32> CVarCompositeCoreDebugDilationSize(
	TEXT("CompositeCore.Debug.DilationSize"),
	1,
	TEXT("Size of the pixel dilation applied onto the composite custom render pass. 0, 1 & 2 are supported."),
	ECVF_Default);

namespace UE
{
	namespace CompositeCore
	{
		FPassOutput::FPassOutput(FScreenPassTexture InTexture, FResourceMetadata InMetadata, TOptional<ResourceId> InOverride)
			: Resource({ MoveTemp(InTexture), MoveTemp(InMetadata) })
			, Override(InOverride)
		{
		}

		FBuiltInRenderPassOptions::FBuiltInRenderPassOptions()
			: ViewUserFlagsOverride{}
			, bEnableUnlitViewmode{true}
			, DilationSize{ CVarCompositeCoreDebugDilationSize.GetValueOnGameThread() }
			, bOpacifyOutput{true}
		{
		}

		const FPostRenderWork& FPostRenderWork::GetDefault()
		{
			static FPostRenderWork DefaultRenderWork = []() -> FPostRenderWork
				{
					FPostRenderWork RenderWork;
					TArray<const FCompositeCorePassProxy*>& SSRInputWork = RenderWork.FramePasses.Add(ISceneViewExtension::EPostProcessingPass::SSRInput);
					TArray<const FCompositeCorePassProxy*>& AfterTonemapWork = RenderWork.FramePasses.Add(ISceneViewExtension::EPostProcessingPass::Tonemap);

					// Default primary input
					FPassInputDecl Input0;
					Input0.Set<FPassInternalResourceDesc>({});
					
					// Built-in custom render pass as secondary input
					FPassInputDecl Input1;
					Input1.Set<FPassExternalResourceDesc>({ BUILT_IN_CRP_ID });

					const FPassInputDeclArray PassDeclaredInputs = { Input0, Input1 };

					SSRInputWork.Add(RenderWork.FrameAllocator->Create<FMergePassProxy>(PassDeclaredInputs));
					AfterTonemapWork.Add(RenderWork.FrameAllocator->Create<FMergePassProxy>(PassDeclaredInputs));

					return RenderWork;
				}();

			return DefaultRenderWork;
		}

		FPostRenderWork::FPostRenderWork()
			: FramePasses{}
			, FrameAllocator{new FSceneRenderingBulkObjectAllocator}
		{ }
	}
}


FCompositeCorePassProxy::FCompositeCorePassProxy(UE::CompositeCore::EDefaultPassInputDecl)
	: PassDeclaredInputs({ UE::CompositeCore::FPassInputDecl(TInPlaceType<UE::CompositeCore::FPassInternalResourceDesc>(), UE::CompositeCore::FPassInternalResourceDesc{}) })
	, PassOutputOverride()
{
}

FCompositeCorePassProxy::FCompositeCorePassProxy(UE::CompositeCore::FPassInputDeclArray InPassDeclaredInputs, TOptional<UE::CompositeCore::ResourceId> InPassOutputOverride)
	: PassDeclaredInputs(MoveTemp(InPassDeclaredInputs))
	, PassOutputOverride(MoveTemp(InPassOutputOverride))
{ }


void FCompositeCorePassProxy::AddSubPasses(int32 InputIndex, TArray<const FCompositeCorePassProxy*> InSubPasses)
{
	if (!InSubPasses.IsEmpty())
	{
		SubPasses.Emplace(InputIndex, MoveTemp(InSubPasses));
	}
}

FScreenPassRenderTarget FCompositeCorePassProxy::CreateOutputRenderTarget(FRDGBuilder& GraphBuilder,
	const FSceneView& InView, const FIntRect& OutputViewRect, FRDGTextureDesc OutputDesc, const TCHAR* InName)
{
	OutputDesc.Format	= FSceneTexturesConfig::Get().ColorFormat;
	OutputDesc.NumMips	= 1;
	OutputDesc.Depth	= 1;
	OutputDesc.Flags	= TexCreate_RenderTargetable | TexCreate_ShaderResource; // TexCreate_UAV;
	OutputDesc.Extent	= OutputViewRect.Size();

	return FScreenPassRenderTarget(GraphBuilder.CreateTexture(OutputDesc, InName), OutputViewRect, InView.GetOverwriteLoadAction());
}

UE::CompositeCore::FPassInputArray::FPassInputArray(
	FRDGBuilder& GraphBuilder,
	const FSceneView& InView,
	const FPostProcessMaterialInputs& InPostInputs,
	const ISceneViewExtension::EPostProcessingPass& InLocation)
{
	using namespace UE::CompositeCore;

	Inputs.Reserve(kPostProcessMaterialInputCountMax);

	{
		FPassInput& Input = Inputs.AddDefaulted_GetRef();
		Input.Texture = FScreenPassTexture::CopyFromSlice(GraphBuilder, InPostInputs.GetInput(EPostProcessMaterialInput::SceneColor));
		Input.Metadata.bInvertedAlpha = true;
		Input.Metadata.bPreExposed = true;
		
		// Note: This assumes the lens file is using the "SVE" method, the PPM one isn't engine-registered.
		const FLensDistortionLUT& LensDistortionLUT = LensDistortion::GetLUTUnsafe(InView);
		Input.Metadata.bDistorted = LensDistortionLUT.IsEnabled()
			&& (LensDistortion::GetPassLocationUnsafe(InView) == LensDistortion::EPassLocation::TSR)
			&& InLocation >= ISceneViewExtension::EPostProcessingPass::SSRInput;

		// After-tonemap scene color may have encoding manually applied, as opposed to _SRGB textures
		if (InLocation >= ISceneViewExtension::EPostProcessingPass::Tonemap)
		{
			if ((InView.Family->EngineShowFlags.Tonemapper == 0) || (InView.Family->EngineShowFlags.PostProcessing == 0))
			{
				Input.Metadata.Encoding = UE::CompositeCore::EEncoding::Gamma;
			}
			else if (InView.Family->SceneCaptureSource == SCS_FinalColorLDR)
			{
				Input.Metadata.Encoding = UE::CompositeCore::EEncoding::sRGB;
			}
		}
	}

	{
		FPassInput& Input = Inputs.AddDefaulted_GetRef();
		Input.Texture = FScreenPassTexture::CopyFromSlice(GraphBuilder, InPostInputs.GetInput(EPostProcessMaterialInput::SeparateTranslucency));
		Input.Metadata.bInvertedAlpha = true;
	}

	{
		FPassInput& Input = Inputs.AddDefaulted_GetRef();
		Input.Texture = FScreenPassTexture::CopyFromSlice(GraphBuilder, InPostInputs.GetInput(EPostProcessMaterialInput::CombinedBloom));
		Input.Metadata.bInvertedAlpha = true;
	}

	{
		FPassInput& Input = Inputs.AddDefaulted_GetRef();
		Input.Texture = FScreenPassTexture::CopyFromSlice(GraphBuilder, InPostInputs.GetInput(EPostProcessMaterialInput::PostTonemapHDRColor));
		Input.Metadata.bInvertedAlpha = true;
	}

	{
		FPassInput& Input = Inputs.AddDefaulted_GetRef();
		Input.Texture = FScreenPassTexture::CopyFromSlice(GraphBuilder, InPostInputs.GetInput(EPostProcessMaterialInput::Velocity));
	}

	OverrideOutput = InPostInputs.OverrideOutput;
}

FPostProcessMaterialInputs UE::CompositeCore::FPassInputArray::ToPostProcessInputs(FRDGBuilder& GraphBuilder, FSceneTextureShaderParameters SceneTextures) const
{
	using namespace UE::CompositeCore;

	FPostProcessMaterialInputs Result;
	for (int32 Index = 0; Index < Inputs.Num(); ++Index)
	{
		const FPassInput& ResolvedInput = Inputs[Index];
		Result.SetInput(static_cast<EPostProcessMaterialInput>(Index), FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, ResolvedInput.Texture));
	}
	Result.SceneTextures = MoveTemp(SceneTextures);

	return Result;
}
