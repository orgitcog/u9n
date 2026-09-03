// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultStereoLayers.h"
#include "HeadMountedDisplayBase.h"

#include "EngineModule.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "RendererInterface.h"
#include "StereoLayerRendering.h"
#include "RHIStaticStates.h"
#include "StaticBoundShaderState.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "SceneView.h"
#include "CommonRenderResources.h"
#include "IXRLoadingScreen.h"
#include "RenderGraphUtils.h"

namespace 
{

	/*=============================================================================
	*
	* Helper functions
	*
	*/

	//=============================================================================
	static FMatrix ConvertTransform(const FTransform& In)
	{

		const FQuat InQuat = In.GetRotation();
		FQuat OutQuat(-InQuat.Y, -InQuat.Z, -InQuat.X, -InQuat.W);

		const FVector InPos = In.GetTranslation();
		FVector OutPos(InPos.Y, InPos.Z, InPos.X);

		const FVector InScale = In.GetScale3D();
		FVector OutScale(InScale.Y, InScale.Z, InScale.X);

		return FTransform(OutQuat, OutPos, OutScale).ToMatrixWithScale() * FMatrix(
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 0, 0, 1));
	}

}

FDefaultStereoLayers::FDefaultStereoLayers(const FAutoRegister& AutoRegister, FHeadMountedDisplayBase* InHMDDevice) 
	: FHMDSceneViewExtension(AutoRegister)
	, HMDDevice(InHMDDevice)
{

}

//=============================================================================

// static
void FDefaultStereoLayers::StereoLayerRender(FRHICommandListImmediate& RHICmdList, TArrayView<const FStereoLayerToRender> LayersToRender, const FDefaultStereoLayers_LayerRenderParams& RenderParams)
{
	check(IsInRenderingThread());
	if (!LayersToRender.Num())
	{
		return;
	}

	IRendererModule& RendererModule = GetRendererModule();
	using TOpaqueBlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>;
	using TAlphaBlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>;

	// Set render state
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None, ERasterizerDepthClipMode::DepthClip, false>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	RHICmdList.SetViewport((float)RenderParams.Viewport.Min.X, (float)RenderParams.Viewport.Min.Y, 0, (float)RenderParams.Viewport.Max.X, (float)RenderParams.Viewport.Max.Y, 1.0f);

	// Set initial shader state
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FStereoLayerVS> VertexShader(ShaderMap);
	TShaderMapRef<FStereoLayerPS> PixelShader(ShaderMap);
	TShaderMapRef<FStereoLayerPS_External> PixelShader_External(ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();

	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	// Force initialization of pipeline state on first iteration:
	bool bLastWasOpaque = (LayersToRender[0].Flags & LAYER_FLAG_TEX_NO_ALPHA_CHANNEL) == 0;
	bool bLastWasExternal = (LayersToRender[0].Flags & LAYER_FLAG_TEX_EXTERNAL) == 0;

	// For each layer
	for (const FStereoLayerToRender& Layer : LayersToRender)
	{
		check(Layer.Texture != nullptr && !(Layer.Flags & LAYER_FLAG_HIDDEN));
		const bool bIsOpaque = (Layer.Flags & LAYER_FLAG_TEX_NO_ALPHA_CHANNEL) != 0;
		const bool bIsExternal = (Layer.Flags & LAYER_FLAG_TEX_EXTERNAL) != 0;
		bool bPipelineStateNeedsUpdate = false;

		if (bIsOpaque != bLastWasOpaque)
		{
			bLastWasOpaque = bIsOpaque;
			GraphicsPSOInit.BlendState = bIsOpaque ? TOpaqueBlendState::GetRHI() : TAlphaBlendState::GetRHI();
			bPipelineStateNeedsUpdate = true;
		}

		if (bIsExternal != bLastWasExternal)
		{
			bLastWasExternal = bIsExternal;
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = bIsExternal ? PixelShader_External.GetPixelShader() : PixelShader.GetPixelShader();
			bPipelineStateNeedsUpdate = true;
		}

		if (bPipelineStateNeedsUpdate)
		{
			// Updater render state
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
		}

		FMatrix LayerMatrix = ConvertTransform(Layer.Transform);

		const FTextureRHIRef Tex = Layer.Texture;
		FVector2D QuadSize = Layer.QuadSize * 0.5f;
		if (Layer.Flags & LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO)
		{
			if (Tex && Tex->GetDesc().Dimension == ETextureDimension::Texture2D)
			{
				const float SizeX = (float)Tex->GetSizeX();
				const float SizeY = (float)Tex->GetSizeY();
				if (SizeX != 0)
				{
					const float AspectRatio = SizeY / SizeX;
					QuadSize.Y = QuadSize.X * AspectRatio;
				}
			}
		}

		// Set shader uniforms
		SetShaderParametersLegacyVS(
			RHICmdList,
			VertexShader,
			QuadSize,
			Layer.UVRect,
			RenderParams.RenderMatrices[static_cast<int>(Layer.PositionType)],
			LayerMatrix);

		SetShaderParametersLegacyPS(
			RHICmdList,
			PixelShader,
			TStaticSamplerState<SF_Trilinear>::GetRHI(),
			Tex,
			bIsOpaque);

		const FIntPoint TargetSize = RenderParams.Viewport.Size();
		// Draw primitive
		RendererModule.DrawRectangle(
			RHICmdList,
			0.0f, 0.0f,
			(float)TargetSize.X, (float)TargetSize.Y,
			0.0f, 0.0f,
			1.0f, 1.0f,
			TargetSize,
			FIntPoint(1, 1),
			VertexShader
		);
	}
}

FDefaultStereoLayers::FStereoLayerToRenderTransfer::FStereoLayerToRenderTransfer(const FLayerDesc& Desc)
	: Id(Desc.Id)
	, Priority(Desc.Priority)
	, Flags(Desc.Flags)
	, PositionType(Desc.PositionType)
	, QuadSize(Desc.QuadSize)
	, UVRect(Desc.UVRect)
	, Transform(Desc.Transform)
	, Texture(Desc.TextureObj.IsValid() ? Desc.TextureObj->GetResource() : nullptr)
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	, Texture_Deprecated(Desc.Texture)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
{
}

FDefaultStereoLayers::FStereoLayerToRender::FStereoLayerToRender(const FStereoLayerToRenderTransfer& Transfer)
	: Id(Transfer.Id)
	, Priority(Transfer.Priority)
	, Flags(Transfer.Flags)
	, PositionType(Transfer.PositionType)
	, QuadSize(Transfer.QuadSize)
	, UVRect(Transfer.UVRect)
	, Transform(Transfer.Transform)
	, Texture(Transfer.Texture ? Transfer.Texture->GetTextureRHI() : Transfer.Texture_Deprecated)
{
}

void FDefaultStereoLayers::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (!GetStereoLayersDirty())
	{
		return;
	}
	
	// Sort layers
	TArray<FStereoLayerToRenderTransfer> SceneLayers;
	TArray<FStereoLayerToRenderTransfer> OverlayLayers;

	ForEachLayer([&](uint32 /* unused */, const FLayerDesc& Layer)
	{
		if (!Layer.IsVisible())
		{
			return;
		}
		if (Layer.PositionType == ELayerType::FaceLocked)
		{
			OverlayLayers.Add(Layer);
		}
		else
		{
			SceneLayers.Add(Layer);
		}
	});

	auto SortLayersPredicate = [&](const FStereoLayerToRenderTransfer& A, const FStereoLayerToRenderTransfer& B)
	{
		return A.Priority < B.Priority;
	};
	SceneLayers.Sort(SortLayersPredicate);
	OverlayLayers.Sort(SortLayersPredicate);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bool bClearLayerBackgroundThisFrame = bSplashIsShown || !IsBackgroundLayerVisible();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ENQUEUE_RENDER_COMMAND(FDefaultStereoLayers_CommitLayersToRender)(
		[this, SceneLayers = MoveTemp(SceneLayers), OverlayLayers = MoveTemp(OverlayLayers), bClearLayerBackgroundThisFrame](FRHICommandListImmediate& RHICmdList) mutable
		{
			SortedSceneLayers.Reset(SceneLayers.Num());
			SortedSceneLayers.Append(SceneLayers);
			SortedOverlayLayers.Reset(OverlayLayers.Num());
			SortedOverlayLayers.Append(OverlayLayers);
			bClearLayerBackground = bClearLayerBackgroundThisFrame;
		});
}


BEGIN_SHADER_PARAMETER_STRUCT(FRenderLayersPass,)
	RDG_TEXTURE_ACCESS_ARRAY(LayerTextures)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void FDefaultStereoLayers::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	if (!IStereoRendering::IsStereoEyeView(InView))
	{
		return;
	}

	FIntRect RenderRect = InView.UnscaledViewRect;
	FTextureRHIRef RenderTarget = HMDDevice->GetSceneLayerTarget_RenderThread(InView.StereoViewIndex, RenderRect);
	if (!RenderTarget.IsValid())
	{
		RenderTarget = InView.Family->RenderTarget->GetRenderTargetTexture();
	}

	FIntRect OverlayRenderRect = RenderRect;
	FTextureRHIRef OverlayRenderTarget = HMDDevice->GetOverlayLayerTarget_RenderThread(InView.StereoViewIndex, OverlayRenderRect);

	// Optionally render face-locked layers into a non-reprojected target if supported by the HMD platform
	bool bSeparateOverlayPass = OverlayRenderTarget.IsValid();

	FRenderLayersPass* MainPass = GraphBuilder.AllocParameters<FRenderLayersPass>();
	FRenderLayersPass* OverlayPass = bSeparateOverlayPass ? GraphBuilder.AllocParameters<FRenderLayersPass>() : nullptr;
	for (const FStereoLayerToRender& SceneLayer : SortedSceneLayers)
	{
		FRDGTextureRef RDGTexture = RegisterExternalTexture(GraphBuilder, SceneLayer.Texture, TEXT("StereoLayerTexture"));
		MainPass->LayerTextures.Add(FRDGTextureAccess(RDGTexture, ERHIAccess::SRVGraphics));
	}
	for (const FStereoLayerToRender& OverlayLayer : SortedOverlayLayers)
	{
		FRDGTextureRef RDGTexture = RegisterExternalTexture(GraphBuilder, OverlayLayer.Texture, TEXT("StereoLayerTexture"));
		(OverlayPass ? OverlayPass : MainPass)
			->LayerTextures.Add(FRDGTextureAccess(RDGTexture, ERHIAccess::SRVGraphics));
	}

	FViewMatrices ModifiedViewMatrices = InView.ViewMatrices;
	ModifiedViewMatrices.HackRemoveTemporalAAProjectionJitter();
	const FMatrix& ProjectionMatrix = ModifiedViewMatrices.GetProjectionMatrix();
	const FMatrix& ViewProjectionMatrix = ModifiedViewMatrices.GetViewProjectionMatrix();

	// Calculate a view matrix that only adjusts for eye position, ignoring head position, orientation and world position.
	FVector EyeShift;
	FQuat EyeOrientation;
	HMDDevice->GetRelativeEyePose(IXRTrackingSystem::HMDDeviceId, InView.StereoViewIndex, EyeOrientation, EyeShift);

	FMatrix EyeMatrix = FTranslationMatrix(-EyeShift) * FInverseRotationMatrix(EyeOrientation.Rotator()) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	FQuat HmdOrientation = HmdTransform.GetRotation();
	FVector HmdLocation = HmdTransform.GetTranslation();
	FMatrix TrackerMatrix = FTranslationMatrix(-HmdLocation) * FInverseRotationMatrix(HmdOrientation.Rotator()) * EyeMatrix;

	FDefaultStereoLayers_LayerRenderParams* RenderParams = GraphBuilder.AllocObject<FDefaultStereoLayers_LayerRenderParams>();
	*RenderParams = {
		RenderRect, // Viewport
		{
			ViewProjectionMatrix,				// WorldLocked,
			TrackerMatrix * ProjectionMatrix,	// TrackerLocked,
			EyeMatrix * ProjectionMatrix		// FaceLocked
		}
	};

	FRDGTextureRef MainTarget = RegisterExternalTexture(GraphBuilder, RenderTarget, TEXT("StereoLayerRenderTarget"));
	MainPass->RenderTargets[0] = FRenderTargetBinding(MainTarget, ERenderTargetLoadAction::ELoad);
	GraphBuilder.AddPass(RDG_EVENT_NAME("StereoLayerRender"), MainPass, ERDGPassFlags::Raster,
		[this, RenderParams, OverlayRenderRect, bSeparateOverlayPass](FRHICommandListImmediate& RHICmdList) {

		RHICmdList.SetViewport((float)RenderParams->Viewport.Min.X, (float)RenderParams->Viewport.Min.Y, 0.0f, (float)RenderParams->Viewport.Max.X, (float)RenderParams->Viewport.Max.Y, 1.0f);

		if (bClearLayerBackground)
		{
			DrawClearQuad(RHICmdList, FLinearColor::Black);
		}

		StereoLayerRender(RHICmdList, SortedSceneLayers, *RenderParams);

		if (!bSeparateOverlayPass)
		{
			RenderParams->Viewport = OverlayRenderRect;
			StereoLayerRender(RHICmdList, SortedOverlayLayers, *RenderParams);
		}
	});
	
	if (bSeparateOverlayPass && OverlayPass)
	{
		FRDGTextureRef OverlayTarget = RegisterExternalTexture(GraphBuilder, OverlayRenderTarget, TEXT("StereoLayerOverlayRenderTarget"));
		OverlayPass->RenderTargets[0] = FRenderTargetBinding(OverlayTarget, ERenderTargetLoadAction::ELoad);
		GraphBuilder.AddPass(RDG_EVENT_NAME("StereoLayerRenderIntoOverlay"), OverlayPass, ERDGPassFlags::Raster,
			[this, RenderParams, OverlayRenderRect, bSeparateOverlayPass](FRHICommandListImmediate& RHICmdList) {

			RenderParams->Viewport = OverlayRenderRect;

			DrawClearQuad(RHICmdList, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			RHICmdList.SetViewport((float)RenderParams->Viewport.Min.X, (float)RenderParams->Viewport.Min.Y, 0.0f, (float)RenderParams->Viewport.Max.X, (float)RenderParams->Viewport.Max.Y, 1.0f);

			StereoLayerRender(RHICmdList, SortedOverlayLayers, *RenderParams);
		});
	}
}

TArray<FTextureRHIRef, TInlineAllocator<2>> FDefaultStereoLayers::GetDebugLayerTexturesImpl_RenderThread()
{
	TArray<FTextureRHIRef, TInlineAllocator<2>> DebugLayers;
	auto CheckAddLayer = [&](const FStereoLayerToRender& Layer)
	{
		if (Layer.Flags & LAYER_FLAG_DEBUG &&
			Layer.Texture.IsValid() &&
			Layer.Texture->GetDesc().Dimension == ETextureDimension::Texture2D)
		{
			DebugLayers.Add(Layer.Texture);
		}
	};

	for (const FStereoLayerToRender& Layer : SortedSceneLayers)
	{
		CheckAddLayer(Layer);
	}
	for (const FStereoLayerToRender& Layer : SortedOverlayLayers)
	{
		CheckAddLayer(Layer);
	}

	return DebugLayers;
}

void FDefaultStereoLayers::GetAllocatedTexture(uint32 LayerId, FTextureRHIRef& Texture, FTextureRHIRef& LeftTexture)
{
	check(IsInRenderingThread());

	Texture = nullptr;
	LeftTexture = nullptr;

	for (const FStereoLayerToRender& Layer : SortedSceneLayers)
	{
		if (Layer.Id == LayerId)
		{
			Texture = Layer.Texture;
			return;
		}
	}
	for (const FStereoLayerToRender& Layer : SortedOverlayLayers)
	{
		if (Layer.Id == LayerId)
		{
			Texture = Layer.Texture;
			return;
		}
	}
}

TArray<FTextureRHIRef, TInlineAllocator<2>> FDefaultStereoLayers::GetDebugLayerTextures_RenderThread()
{
	// Emulated layer support means that the debug layer will be in the 3d scene render that the spectator screen displays.
	//return GetDebugLayerTexturesImpl_RenderThread();
	return {};
}


void FDefaultStereoLayers::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	// Initialize HMD position.
	FQuat HmdOrientation = FQuat::Identity;
	FVector HmdPosition = FVector::ZeroVector;
	HMDDevice->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HmdOrientation, HmdPosition);
	HmdTransform = FTransform(HmdOrientation, HmdPosition);
}
