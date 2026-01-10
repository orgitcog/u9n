// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerViewExtension.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewportProxyData.h"

#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManagerProxy.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Containers/IDisplayClusterRender_MeshComponent.h"
#include "Render/Containers/IDisplayClusterRender_MeshComponentProxy.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"
#include "IDisplayClusterShaders.h"
#include "IDisplayClusterShadersTextureUtils.h"
#include "TextureResource.h"

#include "RHIStaticStates.h"

#include "RenderResource.h"
#include "RenderingThread.h"
#include "CommonRenderResources.h"
#include "PixelShaderUtils.h"

#include "ClearQuad.h"

#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"

#include "ScreenRendering.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"

// Tile rect border width
int32 GDisplayClusterRenderTileBorder = 0;
static FAutoConsoleVariableRef CVarDisplayClusterRenderTileBorder(
	TEXT("nDisplay.render.TileBorder"),
	GDisplayClusterRenderTileBorder,
	TEXT("Tile border width in pixels (default 0).\n"),
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////////////////////////
namespace UE::DisplayCluster::ViewportProxy
{
	// The viewport override has the maximum depth. This protects against a link cycle
	static const int32 DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax = 4;
};

void FDisplayClusterViewportProxy::FillTextureWithColor_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* InRenderTargetTexture, const FLinearColor& InColor)
{
	if (InRenderTargetTexture)
	{
		FRHIRenderPassInfo RPInfo(InRenderTargetTexture, ERenderTargetActions::DontLoad_Store);
		RHICmdList.Transition(FRHITransitionInfo(InRenderTargetTexture, ERHIAccess::Unknown, ERHIAccess::RTV));
		RHICmdList.BeginRenderPass(RPInfo, TEXT("nDisplay_FillTextureWithColor"));
		{
			const FIntPoint Size = InRenderTargetTexture->GetSizeXY();
			RHICmdList.SetViewport(0, 0, 0.0f, Size.X, Size.Y, 1.0f);
			DrawClearQuad(RHICmdList, FLinearColor::Black);
		}
		RHICmdList.EndRenderPass();
		RHICmdList.Transition(FRHITransitionInfo(InRenderTargetTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	}
}

bool FDisplayClusterViewportProxy::ImplGetResources_RenderThread(const EDisplayClusterViewportResourceType InExtResourceType, TArray<FRHITexture*>& OutResources, const int32 InRecursionDepth) const
{
	using namespace UE::DisplayCluster::ViewportProxy;
	check(IsInRenderingThread());

	const EDisplayClusterViewportResourceType InResourceType = GetResourceType_RenderThread(InExtResourceType);

	// Override resources from other viewport
	if (ShouldOverrideViewportResource(InResourceType))
	{
		if (InRecursionDepth < DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax)
		{
			return GetRenderingViewportProxy().ImplGetResources_RenderThread(InExtResourceType, OutResources, InRecursionDepth + 1);
		}

		return false;
	}

	OutResources.Empty();

	switch (InResourceType)
	{
	case EDisplayClusterViewportResourceType::InternalRenderTargetEntireRectResource:
	case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
	{
		bool bResult = false;

		if (Contexts.Num() > 0)
		{

			// 1. Replace RTT from configuration
			if (!bResult && PostRenderSettings.Replace.IsEnabled())
			{
				bResult = true;

				// Support texture replace:
				if (FRHITexture* ReplaceTextureRHI = PostRenderSettings.Replace.TextureRHI->GetTexture2D())
				{
					for (int32 ContextIndex = 0; ContextIndex < Contexts.Num(); ContextIndex++)
					{
						OutResources.Add(ReplaceTextureRHI);
					}
				}
			}

			// 2. Replace RTT from UVLightCard:
			if (!bResult && EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
			{
				bResult = true;

				// Get resources from external UV LightCard manager
				if (FDisplayClusterViewportManagerProxy* ViewportManagerProxy = ConfigurationProxy->GetViewportManagerProxyImpl())
				{
					TSharedPtr<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe> LightCardManager = ViewportManagerProxy->GetLightCardManagerProxy_RenderThread();
					if (LightCardManager.IsValid())
					{
						const EDisplayClusterUVLightCardType UVLightCardType =
							EnumHasAllFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::OverInFrustum)
							? EDisplayClusterUVLightCardType::Over : EDisplayClusterUVLightCardType::Under;

						if (FRHITexture* UVLightCardRHIResource = LightCardManager->GetUVLightCardRHIResource_RenderThread(UVLightCardType))
						{
							for (int32 ContextIndex = 0; ContextIndex < Contexts.Num(); ContextIndex++)
							{
								OutResources.Add(UVLightCardRHIResource);
							}
						}
					}
				}
			}

			// 3. Finally Use InternalRTT
			if (!bResult)
			{
				bResult = Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::RenderTargets, OutResources);
			}
		}

		if (!bResult || Contexts.Num() != OutResources.Num())
		{
			OutResources.Empty();
		}

		return OutResources.Num() > 0;
	}

	case EDisplayClusterViewportResourceType::InputShaderResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::InputShaderResources, OutResources);

	case EDisplayClusterViewportResourceType::AdditionalTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::AdditionalTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::MipsShaderResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::MipsShaderResources, OutResources);

	case EDisplayClusterViewportResourceType::OutputFrameTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::OutputFrameTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::AdditionalFrameTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::AdditionalFrameTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::OutputPreviewTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::OutputPreviewTargetableResources, OutResources);

	default:
		break;
	}

	return false;
}

bool FDisplayClusterViewportProxy::ImplGetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InExtResourceType, TArray<FRHITexture*>& OutResources, TArray<FIntRect>& OutResourceRects, const int32 InRecursionDepth) const
{
	using namespace UE::DisplayCluster::ViewportProxy;
	check(IsInRenderingThread());

	// Override resources from other viewport
	if (ShouldOverrideViewportResource(InExtResourceType))
	{
		if (InRecursionDepth < DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax)
		{
			return GetRenderingViewportProxy().ImplGetResourcesWithRects_RenderThread(InExtResourceType, OutResources, OutResourceRects, InRecursionDepth + 1);
		}

		return false;
	}

	const EDisplayClusterViewportResourceType InResourceType = GetResourceType_RenderThread(InExtResourceType);
	if (!GetResources_RenderThread(InResourceType, OutResources))
	{
		return false;
	}

	// Collect all resource rects:
	for (int32 ContextIt = 0; ContextIt < OutResources.Num(); ContextIt++)
	{
		FIntRect ResourceRect = GetResourceRect_RenderThread(InResourceType, ContextIt);

		// Rect({0,0}, {0,0} means we are using the entire texture.
		if (ResourceRect.IsEmpty() && OutResources[ContextIt])
		{
			ResourceRect.Max = OutResources[ContextIt]->GetDesc().Extent;
		}

		OutResourceRects.Add(ResourceRect);
	}

	return true;
}

bool FDisplayClusterViewportProxy::ImplResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, FDisplayClusterViewportProxy const* SourceProxy, const EDisplayClusterViewportResourceType InExtResourceType, const EDisplayClusterViewportResourceType OutExtResourceType, const int32 InContextNum) const
{
	using namespace UE::DisplayCluster::ViewportProxy;

	check(IsInRenderingThread());
	check(SourceProxy);

	const EDisplayClusterViewportResourceType InResourceType = SourceProxy->GetResourceType_RenderThread(InExtResourceType);
	const EDisplayClusterViewportResourceType OutResourceType = GetResourceType_RenderThread(OutExtResourceType);

	if (InResourceType == EDisplayClusterViewportResourceType::MipsShaderResource)
	{
		// RenderTargetMips not allowved for resolve op
		return false;
	}

	FDisplayClusterShadersTextureUtilsSettings TextureUtilsSettings;
	// The mode used to blend textures
	if (OutResourceType == EDisplayClusterViewportResourceType::OutputPreviewTargetableResource)
	{
		// The preview texture should use only RGB colors and ignore the alpha channel.
		// The alpha channel may or may not be inverted in third-party libraries.
		TextureUtilsSettings.OverrideAlpha = EDisplayClusterShaderTextureUtilsOverrideAlpha::Set_Alpha_One;
	}

	TSharedRef<IDisplayClusterShadersTextureUtils> TextureUtils =
		GetShadersAPI().CreateTextureUtils_RenderThread(RHICmdList)
			->SetInput(SourceProxy, InExtResourceType)
			->SetOutput(this, OutExtResourceType);

	if (InExtResourceType == EDisplayClusterViewportResourceType::AfterWarpBlendTargetableResource
		&& OutExtResourceType == EDisplayClusterViewportResourceType::OutputTargetableResource
		&& DisplayDeviceProxy.IsValid()
		&& DisplayDeviceProxy->HasFinalPass_RenderThread())
	{
		// Custom resolve at external Display Device
		DisplayDeviceProxy->AddFinalPass_RenderThread(TextureUtilsSettings, TextureUtils);
	}
	else
	{
		// Standard resolve:
		TextureUtils->Resolve(TextureUtilsSettings);
	}

	return true;
}

void FDisplayClusterViewportProxy::ImplResolveTileResource_RenderThread(FRHICommandListImmediate& RHICmdList, FDisplayClusterViewportProxy const* InDestViewportProxy) const
	{
	check(InDestViewportProxy);

	using namespace UE::DisplayCluster::ViewportProxy;

	const TArray<FDisplayClusterViewport_Context>& DestContexts = InDestViewportProxy->GetContexts_RenderThread();
	const TArray<FDisplayClusterViewport_Context>& SrcContexts = Contexts;
	TSharedRef<IDisplayClusterShadersTextureUtils> TextureUtils = GetShadersAPI().CreateTextureUtils_RenderThread(RHICmdList);

	TextureUtils
		->SetInput(this, EDisplayClusterViewportResourceType::InternalRenderTargetResource)
		->SetOutput(InDestViewportProxy, EDisplayClusterViewportResourceType::InternalRenderTargetResource)
		->ForEachContextByPredicate(
			EDisplayClusterShaderTextureUtilsFlags::DisableUpdateResourcesRectsForResolve, [&](
			const FDisplayClusterShadersTextureViewportContext& InputContext,
			const FDisplayClusterShadersTextureViewportContext& OutputContext)
			{
				const uint32 ContextNum = InputContext.ContextNum;
				if(!SrcContexts.IsValidIndex(ContextNum) || !DestContexts.IsValidIndex(ContextNum) || OutputContext.ContextNum != ContextNum)
				{
					return;
				}

				FDisplayClusterShadersTextureViewport Input(InputContext);
				FDisplayClusterShadersTextureViewport Output(OutputContext);

				Output.Rect = SrcContexts[ContextNum].TileDestRect;

				if (GDisplayClusterRenderTileBorder > 0)
				{
					// The maximum border is 1/4 of the minimum side of the rectangle.
					const int32 MaxBorderSize = FMath::Min(Input.Rect.Size().GetMin(), Output.Rect.Size().GetMin()) / 4;
					const int32 TileBorderSize = FMath::Min(GDisplayClusterRenderTileBorder, MaxBorderSize);

					// Shows borders for internal areas only
					FIntRect SubRect(FIntPoint::ZeroValue, FIntPoint::ZeroValue);
					if (RenderSettings.TileSettings.GetPos().X > 0)
					{
						SubRect.Min.X = TileBorderSize;
					}
					if (RenderSettings.TileSettings.GetPos().Y > 0)
					{
						SubRect.Min.Y = TileBorderSize;
					}
					if ((RenderSettings.TileSettings.GetPos().X + 1) < RenderSettings.TileSettings.GetSize().X)
					{
						SubRect.Max.X = TileBorderSize;
					}
					if ((RenderSettings.TileSettings.GetPos().Y + 1) < RenderSettings.TileSettings.GetSize().Y)
					{
						SubRect.Max.Y = TileBorderSize;
					}

					// Set rect smaller to show gaps between tiles:
					Input.Rect.Min.X += SubRect.Min.X;
					Input.Rect.Min.Y += SubRect.Min.Y;
					Input.Rect.Max.X -= SubRect.Min.X + SubRect.Max.X;
					Input.Rect.Max.Y -= SubRect.Min.Y + SubRect.Max.Y;

					Output.Rect.Min.X += SubRect.Min.X;
					Output.Rect.Min.Y += SubRect.Min.Y;
					Output.Rect.Max.X -= SubRect.Min.X + SubRect.Max.X;
					Output.Rect.Max.Y -= SubRect.Min.Y + SubRect.Max.Y;
				}

				// Copy texture region
				TextureUtils->ResolveTextureContext(
					EDisplayClusterShaderTextureUtilsFlags::DisableResampleShader,
					Input, Output);
			});
}

void FDisplayClusterViewportProxy::CleanupResources_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	// Since the RTT is reused through frames, in case we need to show a black border between viewport tiles, we must fill the original viewport with this colour.
	if (GDisplayClusterRenderTileBorder > 0 && RenderSettings.TileSettings.GetType() == EDisplayClusterViewportTileType::Source)
	{
		TArray<FRHITexture*> RenderTargets;
		if (GetResources_RenderThread(EDisplayClusterViewportResourceType::InternalRenderTargetEntireRectResource, RenderTargets))
		{
			for (FRHITexture* TextureIt : RenderTargets)
			{
				// Note: It may make sense to move the CVar and border color to the StageSettings.
				FDisplayClusterViewportProxy::FillTextureWithColor_RenderThread(RHICmdList, TextureIt, FLinearColor::Black);
			}
		}
	}
}
