// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultSpectatorScreenController.h"
#include "HeadMountedDisplayTypes.h"
#include "PipelineStateCache.h"
#include "TextureResource.h"
#include "GlobalRenderResources.h"
#include "Misc/CoreDelegates.h"
#include "Engine/Texture.h"
#include "HeadMountedDisplayBase.h"
#include "SceneUtils.h" // for SCOPED_DRAW_EVENT()
#include "IStereoLayers.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "SystemTextures.h"
#include "Engine/Engine.h"
#include "XRCopyTexture.h"
#include "SceneView.h"


FDefaultSpectatorScreenController::FDefaultSpectatorScreenController(FHeadMountedDisplayBase* InHMDDevice)
	: FeatureLevel_RenderThread(GMaxRHIFeatureLevel)
	, ShaderPlatform_RenderThread(GMaxRHIShaderPlatform)
	, HMDDevice(InHMDDevice)
{
}


ESpectatorScreenMode FDefaultSpectatorScreenController::GetSpectatorScreenMode() const
{
	check(IsInGameThread());

	return SpectatorScreenMode_GameThread;
}

void FDefaultSpectatorScreenController::SetSpectatorScreenMode(ESpectatorScreenMode Mode)
{
	check(IsInGameThread());

	UE_LOG(LogHMD, Log, TEXT("SetSpectatorScreenMode(%i)."), static_cast<int32>(Mode));

	SpectatorScreenMode_GameThread = Mode;

	ENQUEUE_RENDER_COMMAND(SetSpectatorScreenMode)(
		[this, Mode](FRHICommandListImmediate&)
		{
			SpectatorScreenMode_RenderThread = Mode;
		});
}

void FDefaultSpectatorScreenController::SetSpectatorScreenTexture(UTexture* SrcTexture)
{
	SpectatorScreenTexture = SrcTexture;
}

UTexture* FDefaultSpectatorScreenController::GetSpectatorScreenTexture() const
{
	if (SpectatorScreenTexture.IsValid())
	{
		return SpectatorScreenTexture.Get();
	}
	return nullptr;
}

void FDefaultSpectatorScreenController::SetSpectatorScreenModeTexturePlusEyeLayout(const FSpectatorScreenModeTexturePlusEyeLayout& Layout)
{
	if (Layout.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(SetSpectatorScreenModeTexturePlusEyeLayout)(
			[this, Layout](FRHICommandListImmediate&)
			{
				SpectatorScreenModeTexturePlusEyeLayout_RenderThread = Layout;
			});
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("SetSpectatorScreenModeTexturePlusEyeLayout called with invalid Layout.  Ignoring it.  See warnings above."))
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FSpectatorScreenRenderDelegate* FDefaultSpectatorScreenController::GetSpectatorScreenRenderDelegate_RenderThread()
{
	return &SpectatorScreenDelegate_RenderThread;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FDefaultSpectatorScreenController::BeginRenderViewFamily(FSceneViewFamily& ViewFamily)
{
	check(IsInGameThread());

	UTexture* Texture = SpectatorScreenTexture.Get();
	FTextureResource* TextureResource = Texture ? Texture->GetResource() : nullptr;
	ERHIFeatureLevel::Type FeatureLevel = ViewFamily.GetFeatureLevel();
	EShaderPlatform ShaderPlatform = ViewFamily.GetShaderPlatform();

	ENQUEUE_RENDER_COMMAND(SetSpectatorScreenTexture)(
		[this, TextureResource, FeatureLevel, ShaderPlatform](FRHICommandListImmediate& RHICmdList)
		{
			FeatureLevel_RenderThread = FeatureLevel;
			ShaderPlatform_RenderThread = ShaderPlatform;
			SpectatorScreenTexture_RenderThread = TextureResource;
		});
}

// It is imporant that this function be called early in the render frame, ie in PreRenderViewFamily_RenderThread so that
// SpectatorScreenMode_RenderThread is set before other render frame work is done.
void FDefaultSpectatorScreenController::UpdateSpectatorScreenMode_RenderThread()
{
	check(IsInRenderingThread());

	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	if (DelegateSpectatorScreenMode_RenderThread == SpectatorScreenMode_RenderThread)
	{
		return;
	}

	FSpectatorScreenRenderDelegate* RenderDelegate = GetSpectatorScreenRenderDelegate_RenderThread();
	check(RenderDelegate);

	RenderDelegate->Unbind();

	DelegateSpectatorScreenMode_RenderThread = SpectatorScreenMode_RenderThread;

	switch (DelegateSpectatorScreenMode_RenderThread)
	{
	case ESpectatorScreenMode::Disabled:
		break;
	case ESpectatorScreenMode::SingleEyeLetterboxed:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeSingleEyeLetterboxed);
		break;
	case ESpectatorScreenMode::Undistorted:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeUndistorted);
		break;
	case ESpectatorScreenMode::Distorted:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeDistorted);
		break;
	case ESpectatorScreenMode::SingleEye:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeSingleEye);
		break;
	case ESpectatorScreenMode::Texture:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeTexture);
		break;
	case ESpectatorScreenMode::TexturePlusEye:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeMirrorAndTexture);
		break;
	default:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeSingleEyeCroppedToFill);
		break;
	}

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FDefaultSpectatorScreenController::RenderSpectatorScreen_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* BackBuffer, FTextureRHIRef SrcTexture, FVector2D WindowSize)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RenderSpectatorScreen_RenderThread(RHICmdList, BackBuffer, SrcTexture, nullptr, WindowSize);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FDefaultSpectatorScreenController::RenderSpectatorScreen_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* BackBuffer, FTextureRHIRef SrcTexture, FTextureRHIRef LayersTexture, FVector2D WindowSize)
{
	SCOPED_NAMED_EVENT_TEXT("RenderSocialScreen_RenderThread()", FColor::Magenta);

	check(IsInRenderingThread());

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StereoLayersTexture = LayersTexture;

	if (SpectatorScreenDelegate_RenderThread.IsBound())
	{
		SCOPED_DRAW_EVENT(RHICmdList, SpectatorScreen)
		SpectatorScreenDelegate_RenderThread.Execute(RHICmdList, BackBuffer, SrcTexture, SpectatorScreenTexture_RenderThread->GetTextureRHI(), WindowSize);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Apply the debug canvas layer.
	IStereoLayers* StereoLayers = HMDDevice->GetStereoLayers();
	if (StereoLayers && !LayersTexture)
	{
		const FIntRect DstRect(0, 0, BackBuffer->GetSizeX(), BackBuffer->GetSizeY());

		for (FTextureRHIRef LayerTexture : StereoLayers->GetDebugLayerTextures_RenderThread())
		{
			FTextureRHIRef LayerTexture2D = LayerTexture->GetTexture2D();
			check(LayerTexture2D.IsValid());  // Debug canvas layer should be a 2d layer
			const FIntRect LayerRect(0, 0, LayerTexture2D->GetSizeX(), LayerTexture2D->GetSizeY());
			const FIntRect DstRectLetterboxed = Helpers::GetLetterboxedDestRect(LayerRect, DstRect);
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			HMDDevice->CopyTexture_RenderThread(RHICmdList, LayerTexture2D, LayerRect, BackBuffer, DstRectLetterboxed, false, false);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	}
}

void FDefaultSpectatorScreenController::RenderSpectatorScreen_RenderThread(FRDGBuilder& GraphBuilder, FRDGTextureRef BackBuffer, FRDGTextureRef SrcTexture, FRDGTextureRef LayersTexture, FVector2f WindowSize)
{
	SCOPED_NAMED_EVENT_TEXT("RenderSocialScreen_RenderThread()", FColor::Magenta);

	check(IsInRenderingThread());

	StereoLayersTextureRDG = LayersTexture;
	ON_SCOPE_EXIT{ StereoLayersTextureRDG = nullptr; };

	FRDGTextureRef OtherTexture = nullptr;
	if (SpectatorScreenTexture_RenderThread)
	{
		OtherTexture = RegisterExternalTexture(GraphBuilder, SpectatorScreenTexture_RenderThread->GetTextureRHI(), TEXT("DefaultSpectatorScreen_OtherTexture"));
	}
	AddSpectatorModePass(SpectatorScreenMode_RenderThread, GraphBuilder, BackBuffer, SrcTexture, OtherTexture, WindowSize);

	// Apply the debug canvas layer.
	IStereoLayers* StereoLayers = HMDDevice->GetStereoLayers();
	if (StereoLayers && !LayersTexture)
	{
		const FIntRect DstRect(0, 0, BackBuffer->Desc.GetSize().X, BackBuffer->Desc.GetSize().Y);

		FXRCopyTextureOptions Options(FeatureLevel_RenderThread, ShaderPlatform_RenderThread);
		Options.BlendMod = EXRCopyTextureBlendModifier::PremultipliedAlphaBlend;
		Options.SetDisplayMappingOptions(HMDDevice->GetRenderTargetManager());
		for (FTextureRHIRef LayerTexture : StereoLayers->GetDebugLayerTextures_RenderThread())
		{
			FTextureRHIRef LayerTexture2D = LayerTexture->GetTexture2D();
			check(LayerTexture2D.IsValid());  // Debug canvas layer should be a 2d layer
			const FIntRect LayerRect(0, 0, LayerTexture2D->GetSizeX(), LayerTexture2D->GetSizeY());
			const FIntRect DstRectLetterboxed = Helpers::GetLetterboxedDestRect(LayerRect, DstRect);
			FRDGTextureRef RDGLayerTexture = RegisterExternalTexture(GraphBuilder, LayerTexture2D, TEXT("OpenXRSpectatorDebugLayerTexture"));
			AddXRCopyTexturePass(GraphBuilder, RDG_EVENT_NAME("DefaultSpectatorScreen_DebugLayers"), RDGLayerTexture, LayerRect, BackBuffer, DstRectLetterboxed, Options);
		}
	}
}

FIntRect FDefaultSpectatorScreenController::GetFullFlatEyeRect_RenderThread(const FRHITextureDesc& EyeTexture)
{
	return HMDDevice->GetFullFlatEyeRect_RenderThread(EyeTexture);
}

FIntRect FDefaultSpectatorScreenController::GetFullFlatEyeRect_RenderThread(FTextureRHIRef EyeTexture)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return HMDDevice->GetFullFlatEyeRect_RenderThread(EyeTexture);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FDefaultSpectatorScreenController::CopyEmulatedLayers(FRHICommandListImmediate& RHICmdList, FTextureRHIRef TargetTexture, const FIntRect SrcRect, const FIntRect DstRect)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (StereoLayersTexture)
	{
		HMDDevice->CopyTexture_RenderThread(RHICmdList, StereoLayersTexture, SrcRect, TargetTexture, DstRect, false, false); 
	}
	StereoLayersTexture = nullptr;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FDefaultSpectatorScreenController::CopyEmulatedLayers(FRDGBuilder& GraphBuilder, FRDGTextureRef TargetTexture, const FIntRect SrcRect, const FIntRect DstRect)
{
	if (StereoLayersTextureRDG)
	{
		FXRCopyTextureOptions Options(FeatureLevel_RenderThread, ShaderPlatform_RenderThread);
		Options.bClearBlack = false;
		Options.BlendMod = EXRCopyTextureBlendModifier::PremultipliedAlphaBlend;
		Options.SetDisplayMappingOptions(HMDDevice->GetRenderTargetManager());
		AddXRCopyTexturePass(GraphBuilder, RDG_EVENT_NAME("DefaultSpectatorScreen_CopyEmulatedLayers"), StereoLayersTextureRDG, SrcRect, TargetTexture, DstRect, Options);
	}
	StereoLayersTextureRDG = nullptr;
}

void FDefaultSpectatorScreenController::AddSpectatorModePassTexturePlusEye(FRDGBuilder& GraphBuilder, FRDGTextureRef TargetTexture,
	FRDGTextureRef EyeTexture, FRDGTextureRef OtherTexture)
{
	FRDGTextureRef OtherTextureLocal = OtherTexture ? OtherTexture : GetFallbackRDGTexture(GraphBuilder);

	const FIntRect EyeDstRect = SpectatorScreenModeTexturePlusEyeLayout_RenderThread.GetScaledEyeRect(TargetTexture->Desc.GetSize().X, TargetTexture->Desc.GetSize().Y);
	const FIntRect EyeSrcRect = GetFullFlatEyeRect_RenderThread(EyeTexture->Desc);
	const FIntRect CroppedEyeSrcRect = Helpers::GetEyeCroppedToFitRect(HMDDevice->GetEyeCenterPoint_RenderThread(EStereoscopicEye::eSSE_LEFT_EYE), EyeSrcRect, EyeDstRect);

	const FIntRect OtherDstRect = SpectatorScreenModeTexturePlusEyeLayout_RenderThread.GetScaledTextureRect(TargetTexture->Desc.GetSize().X, TargetTexture->Desc.GetSize().Y);
	const FIntRect OtherSrcRect(0, 0, OtherTextureLocal->Desc.GetSize().X, OtherTextureLocal->Desc.GetSize().Y);

	FXRCopyTextureOptions Options(FeatureLevel_RenderThread, ShaderPlatform_RenderThread);
	Options.bClearBlack = SpectatorScreenModeTexturePlusEyeLayout_RenderThread.bClearBlack;
	Options.SetDisplayMappingOptions(HMDDevice->GetRenderTargetManager());

	if (SpectatorScreenModeTexturePlusEyeLayout_RenderThread.bDrawEyeFirst)
	{
		AddXRCopyTexturePass(GraphBuilder, RDG_EVENT_NAME("DefaultSpectatorScreen_TexturePlusEye_EyeTexture1st"), EyeTexture, CroppedEyeSrcRect, TargetTexture, EyeDstRect, Options);
		CopyEmulatedLayers(GraphBuilder, TargetTexture, CroppedEyeSrcRect, EyeDstRect);
		Options.BlendMod = SpectatorScreenModeTexturePlusEyeLayout_RenderThread.bUseAlpha ?
			EXRCopyTextureBlendModifier::PremultipliedAlphaBlend :
			EXRCopyTextureBlendModifier::Opaque;
		AddXRCopyTexturePass(GraphBuilder, RDG_EVENT_NAME("DefaultSpectatorScreen_TexturePlusEye_OtherTexture2nd"), OtherTextureLocal, OtherSrcRect, TargetTexture, OtherDstRect, Options);
	}
	else
	{
		AddXRCopyTexturePass(GraphBuilder, RDG_EVENT_NAME("DefaultSpectatorScreen_TexturePlusEye_OtherTexture1st"), OtherTextureLocal, OtherSrcRect, TargetTexture, OtherDstRect, Options);
		Options.bClearBlack = false;
		AddXRCopyTexturePass(GraphBuilder, RDG_EVENT_NAME("DefaultSpectatorScreen_TexturePlusEye_EyeTexture2nd"), EyeTexture, CroppedEyeSrcRect, TargetTexture, EyeDstRect, Options);
		CopyEmulatedLayers(GraphBuilder, TargetTexture, CroppedEyeSrcRect, EyeDstRect);
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

void FDefaultSpectatorScreenController::RenderSpectatorModeUndistorted(FRHICommandListImmediate& RHICmdList, FTextureRHIRef TargetTexture, FTextureRHIRef EyeTexture, FTextureRHIRef OtherTexture, FVector2D WindowSize)
{
	const FIntRect SrcRect(0, 0, EyeTexture->GetSizeX(), EyeTexture->GetSizeY());
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());

	HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcRect, TargetTexture, DstRect, false, true);
	CopyEmulatedLayers(RHICmdList, TargetTexture, SrcRect, DstRect);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeDistorted(FRHICommandListImmediate& RHICmdList, FTextureRHIRef TargetTexture, FTextureRHIRef EyeTexture, FTextureRHIRef OtherTexture, FVector2D WindowSize)
{
	// Note distorted mode is supported on only on oculus
	// The default implementation falls back to RenderSpectatorModeSingleEyeCroppedToFill.
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage((uint64) this, 2.0, FColor::Red, TEXT("Distorted mode is not available in the default spectator controller."));
	}
	RenderSpectatorModeSingleEyeCroppedToFill(RHICmdList, TargetTexture, EyeTexture, OtherTexture, WindowSize);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeSingleEye(FRHICommandListImmediate& RHICmdList, FTextureRHIRef TargetTexture, FTextureRHIRef EyeTexture, FTextureRHIRef OtherTexture, FVector2D WindowSize)
{
	const FIntRect SrcRect(0, 0, EyeTexture->GetSizeX() / 2, EyeTexture->GetSizeY());
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());

	HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcRect, TargetTexture, DstRect, false, true);
	CopyEmulatedLayers(RHICmdList, TargetTexture, SrcRect, DstRect);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeSingleEyeLetterboxed(FRHICommandListImmediate& RHICmdList, FTextureRHIRef TargetTexture, FTextureRHIRef EyeTexture, FTextureRHIRef OtherTexture, FVector2D WindowSize)
{
	const FIntRect SrcRect = GetFullFlatEyeRect_RenderThread(EyeTexture->GetDesc());
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());
	const FIntRect DstRectLetterboxed = Helpers::GetLetterboxedDestRect(SrcRect, DstRect);

	HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcRect, TargetTexture, DstRectLetterboxed, true, true);
	CopyEmulatedLayers(RHICmdList, TargetTexture, SrcRect, DstRectLetterboxed);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeSingleEyeCroppedToFill(FRHICommandListImmediate& RHICmdList, FTextureRHIRef TargetTexture, FTextureRHIRef EyeTexture, FTextureRHIRef OtherTexture, FVector2D WindowSize)
{
	const FIntRect SrcRect = GetFullFlatEyeRect_RenderThread(EyeTexture->GetDesc());
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());
	const FIntRect WindowRect(0, 0, static_cast<int32>(WindowSize.X), static_cast<int32>(WindowSize.Y));

	const FIntRect SrcCroppedToFitRect = Helpers::GetEyeCroppedToFitRect(HMDDevice->GetEyeCenterPoint_RenderThread(EStereoscopicEye::eSSE_LEFT_EYE), SrcRect, WindowRect);

	HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcCroppedToFitRect, TargetTexture, DstRect, false, true);
	CopyEmulatedLayers(RHICmdList, TargetTexture, SrcCroppedToFitRect, DstRect);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeTexture(FRHICommandListImmediate& RHICmdList, FTextureRHIRef TargetTexture, FTextureRHIRef EyeTexture, FTextureRHIRef OtherTexture, FVector2D WindowSize)
{
	FRHITexture* SrcTexture = OtherTexture;
	if (!SrcTexture)
	{
		SrcTexture = GetFallbackRHITexture();
	}

	const FIntRect SrcRect(0, 0, SrcTexture->GetSizeX(), SrcTexture->GetSizeY());
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());

	HMDDevice->CopyTexture_RenderThread(RHICmdList, SrcTexture, SrcRect, TargetTexture, DstRect, false, true);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeMirrorAndTexture(FRHICommandListImmediate& RHICmdList, FTextureRHIRef TargetTexture, FTextureRHIRef EyeTexture, FTextureRHIRef OtherTexture, FVector2D WindowSize)
{
	FRHITexture* OtherTextureLocal = OtherTexture;
	if (!OtherTextureLocal)
	{
		OtherTextureLocal = GetFallbackRHITexture();
	}

	const FIntRect EyeDstRect = SpectatorScreenModeTexturePlusEyeLayout_RenderThread.GetScaledEyeRect(TargetTexture->GetSizeX(), TargetTexture->GetSizeY());
	const FIntRect EyeSrcRect = GetFullFlatEyeRect_RenderThread(EyeTexture->GetDesc());
	const FIntRect CroppedEyeSrcRect = Helpers::GetEyeCroppedToFitRect(HMDDevice->GetEyeCenterPoint_RenderThread(EStereoscopicEye::eSSE_LEFT_EYE), EyeSrcRect, EyeDstRect);

	const FIntRect OtherDstRect = SpectatorScreenModeTexturePlusEyeLayout_RenderThread.GetScaledTextureRect(TargetTexture->GetSizeX(), TargetTexture->GetSizeY());
	const FIntRect OtherSrcRect(0, 0, OtherTextureLocal->GetSizeX(), OtherTextureLocal->GetSizeY());

	const bool bClearBlack = SpectatorScreenModeTexturePlusEyeLayout_RenderThread.bClearBlack;

	if (SpectatorScreenModeTexturePlusEyeLayout_RenderThread.bDrawEyeFirst)
	{
		HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, CroppedEyeSrcRect, TargetTexture, EyeDstRect, bClearBlack, true);
		CopyEmulatedLayers(RHICmdList, TargetTexture, CroppedEyeSrcRect, EyeDstRect);
		HMDDevice->CopyTexture_RenderThread(RHICmdList, OtherTextureLocal, OtherSrcRect, TargetTexture, OtherDstRect, false, !SpectatorScreenModeTexturePlusEyeLayout_RenderThread.bUseAlpha);
	}
	else
	{
		HMDDevice->CopyTexture_RenderThread(RHICmdList, OtherTextureLocal, OtherSrcRect, TargetTexture, OtherDstRect, bClearBlack, true);
		HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, CroppedEyeSrcRect, TargetTexture, EyeDstRect, false, true);
		CopyEmulatedLayers(RHICmdList, TargetTexture, CroppedEyeSrcRect, EyeDstRect);
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FDefaultSpectatorScreenController::AddSpectatorModePass(ESpectatorScreenMode SpectatorMode, FRDGBuilder& GraphBuilder, FRDGTextureRef TargetTexture, FRDGTextureRef EyeTexture, FRDGTextureRef OtherTexture, FVector2f WindowSize)
{
	// Special cases
	if (SpectatorMode == ESpectatorScreenMode::Disabled)
	{
		return;
	}

	if (SpectatorMode == ESpectatorScreenMode::TexturePlusEye)
	{
		AddSpectatorModePassTexturePlusEye(GraphBuilder, TargetTexture, EyeTexture, OtherTexture);
		return;
	}

	// Standard path
	FIntRect SrcRect;
	FRDGTextureRef SrcTexture = EyeTexture;
	FIntRect DstRect = FIntRect(0, 0, TargetTexture->Desc.GetSize().X, TargetTexture->Desc.GetSize().Y);
	FXRCopyTextureOptions Options(FeatureLevel_RenderThread, ShaderPlatform_RenderThread);
	Options.bClearBlack = false;
	Options.SetDisplayMappingOptions(HMDDevice->GetRenderTargetManager());
	bool bCopyEmulatedLayers = true;
	
	switch (SpectatorMode)
	{
	case ESpectatorScreenMode::SingleEyeLetterboxed:
		SrcRect = GetFullFlatEyeRect_RenderThread(EyeTexture->Desc);
		DstRect = Helpers::GetLetterboxedDestRect(SrcRect, DstRect);
		Options.bClearBlack = true;
		break;
	case ESpectatorScreenMode::Undistorted:
		SrcRect = FIntRect(0, 0, EyeTexture->Desc.GetSize().X, EyeTexture->Desc.GetSize().Y);
		break;
	case ESpectatorScreenMode::SingleEye:
		SrcRect = FIntRect(0, 0, EyeTexture->Desc.GetSize().X / 2, EyeTexture->Desc.GetSize().Y);
		break;
	case ESpectatorScreenMode::Texture:
		SrcTexture = OtherTexture ? OtherTexture : GetFallbackRDGTexture(GraphBuilder);
		SrcRect = FIntRect(0, 0, SrcTexture->Desc.GetSize().X, SrcTexture->Desc.GetSize().Y);
		bCopyEmulatedLayers = false;
		break;
	default:
		// Some modes are only supported by certain plugins
		// The default implementation falls back to SingleEyeCroppedToFill.
		if(GEngine)
		{
			GEngine->AddOnScreenDebugMessage((uint64) this, 2.0, FColor::Red,
				FString::Printf(TEXT("ESpectatorScreenMode %d is not available in the default spectator controller."), (int)SpectatorMode));
		}
		// fall-through
	case ESpectatorScreenMode::SingleEyeCroppedToFill:
		SrcRect = Helpers::GetEyeCroppedToFitRect(HMDDevice->GetEyeCenterPoint_RenderThread(EStereoscopicEye::eSSE_LEFT_EYE),
			GetFullFlatEyeRect_RenderThread(EyeTexture->Desc),
			FIntRect(0, 0, static_cast<int32>(WindowSize.X), static_cast<int32>(WindowSize.Y)));
		break;
	}

	AddXRCopyTexturePass(GraphBuilder, RDG_EVENT_NAME("DefaultSpectatorScreen_CopyTexture"), SrcTexture, SrcRect, TargetTexture, DstRect, Options);
	if (bCopyEmulatedLayers)
	{
		CopyEmulatedLayers(GraphBuilder, TargetTexture, SrcRect, DstRect);
	}
}


FRHITexture* FDefaultSpectatorScreenController::GetFallbackRHITexture() const
{
	//return GWhiteTexture->TextureRHI->GetTexture2D();
	return GBlackTexture->TextureRHI->GetTexture2D();
}

FRDGTextureRef FDefaultSpectatorScreenController::GetFallbackRDGTexture(FRDGBuilder& GraphBuilder) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RegisterExternalTexture(GraphBuilder, GetFallbackRHITexture(), TEXT("DefaultSpectatorScreen_Fallback"));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FIntRect FDefaultSpectatorScreenController::Helpers::GetEyeCroppedToFitRect(FVector2D EyeCenterPoint, const FIntRect& SrcRect, const FIntRect& TargetRect)
{
	// Return a SubRect of EyeRect which has the same aspect ratio as TargetRect
	// such that drawing that SubRect of the eye texture into TargetRect of some other texture
	// will give a nice single eye cropped to fit view.

	// If EyeCenterPoint can be put in the center of the screen by shifting the crop up/down or left/right
	// shift it as far as we can without cropping further.  This means if we are cropping
	// vertically we can shift to a vertical center other than 0.5, and if we are cropping horizontally
	// we can shift to a horizontal center other than 0.5.

	// Eye rect is the subrect of the eye texture that we want to crop to fit TargetRect.
	// Eye rect should already have been cropped to only contain pixels we might want to show on TargetRect.
	// So it ought to be cropped to the reasonably flat-looking part of the rendered area.

	FIntRect OutRect = SrcRect;

	// Assuming neither rect is zero size in any dimension.
	check(SrcRect.Area() != 0);
	check(TargetRect.Area() != 0);

	const float SrcRectAspect = (float)SrcRect.Width() / (float)SrcRect.Height();
	const float TargetRectAspect = (float)TargetRect.Width() / (float)TargetRect.Height();

	if (SrcRectAspect < TargetRectAspect)
	{
		// Source is taller than destination
		// Crop top/bottom
		const float DesiredSrcHeight = SrcRect.Height() * (SrcRectAspect / TargetRectAspect);
		const int32 HalfHeightDiff = FMath::TruncToInt(((float)SrcRect.Height() - DesiredSrcHeight) * 0.5f);
		OutRect.Min.Y += HalfHeightDiff;
		OutRect.Max.Y -= HalfHeightDiff;
		const int32 DesiredCenterAdjustment = FMath::TruncToInt(((float)EyeCenterPoint.Y - 0.5f) * (float)SrcRect.Height());
		const int32 ActualCenterAdjustment = FMath::Clamp(DesiredCenterAdjustment, -HalfHeightDiff, HalfHeightDiff);
		OutRect.Min.Y += ActualCenterAdjustment;
		OutRect.Max.Y += ActualCenterAdjustment;
	}
	else
	{
		// Source is wider than destination
		// Crop left/right
		const float DesiredSrcWidth = SrcRect.Width() * (TargetRectAspect / SrcRectAspect);
		const int32 HalfWidthDiff = FMath::TruncToInt(((float)SrcRect.Width() - DesiredSrcWidth) * 0.5f);
		OutRect.Min.X += HalfWidthDiff;
		OutRect.Max.X -= HalfWidthDiff;
		const int32 DesiredCenterAdjustment = FMath::TruncToInt(((float)EyeCenterPoint.X - 0.5f) * (float)SrcRect.Width());
		const int32 ActualCenterAdjustment = FMath::Clamp(DesiredCenterAdjustment, -HalfWidthDiff, HalfWidthDiff);
		OutRect.Min.X += ActualCenterAdjustment;
		OutRect.Max.X += ActualCenterAdjustment;
	}

	return OutRect;
}

FIntRect FDefaultSpectatorScreenController::Helpers::GetLetterboxedDestRect(const FIntRect& SrcRect, const FIntRect& TargetRect)
{
	FIntRect OutRect = TargetRect;

	// Assuming neither rect is zero size in any dimension.
	check(SrcRect.Area() != 0);
	check(TargetRect.Area() != 0);

	const float SrcRectAspect = (float)SrcRect.Width() / (float)SrcRect.Height();
	const float TargetRectAspect = (float)TargetRect.Width() / (float)TargetRect.Height();

	if (SrcRectAspect < TargetRectAspect)
	{
		// Source is taller than destination
		// Column-boxing
		const float DesiredTgtWidth = TargetRect.Width() * (SrcRectAspect / TargetRectAspect);
		const int32 HalfWidthDiff = FMath::TruncToInt(((float)TargetRect.Width() - DesiredTgtWidth) * 0.5f);
		OutRect.Min.X += HalfWidthDiff;
		OutRect.Max.X -= HalfWidthDiff;
	}
	else
	{
		// Source is wider than destination
		// Letter-boxing
		const float DesiredTgtHeight = TargetRect.Height() * (TargetRectAspect / SrcRectAspect);
		const int32 HalfHeightDiff = FMath::TruncToInt(((float)TargetRect.Height() - DesiredTgtHeight) * 0.5f);
		OutRect.Min.Y += HalfHeightDiff;
		OutRect.Max.Y -= HalfHeightDiff;
	}

	return OutRect;
}
