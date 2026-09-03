// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/DisplayClusterViewport_OpenColorIO.h"
#include "Render/Viewport/DisplayClusterViewportManagerViewExtension.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationProxy.h"
#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManager.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/DisplayClusterViewportStereoscopicPass.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewportProxyData.h"

#include "Render/Viewport/Preview/DisplayClusterViewportPreview.h"

#include "Render/DisplayDevice/Components/DisplayClusterDisplayDeviceBaseComponent.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"

#include "EngineUtils.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneView.h"
#include "UnrealClient.h"

#include "DisplayClusterSceneViewExtensions.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"

#include "LegacyScreenPercentageDriver.h"

#include "Misc/CommandLine.h"
#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/Parse.h"

/** Auxiliary enumeration for CVar “DC.LumenPerView”. */
enum class EDisplayClusterLumenPerView : uint8
{
	// Disabled
	Disabled = 0,

	// Use only in Cluster.
	UseOnlyInCluster,

	// Use in both Cluster and Preview.
	UseEverywhere,

	// Use only in Preview.
	UseOnlyInPreview,

	// Use only for Inner Frustum in Cluster.
	UseOnlyForICVXFCameraInCluster,

	MAX = UseOnlyForICVXFCameraInCluster
};

int32 GDisplayClusterLumenPerView = (uint8)EDisplayClusterLumenPerView::UseOnlyInCluster;
static FAutoConsoleVariableRef CVarDisplayClusterLumenPerView(
	TEXT("DC.LumenPerView"),
	GDisplayClusterLumenPerView,
	TEXT("Separate Lumen scene cache allocated for each View. (Default = 1)\n")
	TEXT("Reduces artifacts where views affect one another, at a cost in GPU memory.\n")
	TEXT(" 0 - Disabled.\n")
	TEXT(" 1 - Use only in Cluster.\n")
	TEXT(" 2 - Use in both Cluster and Preview.\n")
	TEXT(" 3 - Use only in Preview.\n")
	TEXT(" 4 - Use only for Inner Frustum in Cluster.\n"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterPreviewEnableViewState = 1;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewEnableViewState(
	TEXT("nDisplay.preview.EnableViewState"),
	GDisplayClusterPreviewEnableViewState,
	TEXT("Enable view state for preview (0 - disable).\n"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterPreviewEnableConfiguratorViewState = 0;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewEnableConfiguratorViewState(
	TEXT("nDisplay.preview.EnableConfiguratorViewState"),
	GDisplayClusterPreviewEnableConfiguratorViewState,
	TEXT("Enable view state for preview in Configurator window (0 - disable).\n"),
	ECVF_RenderThreadSafe
);


///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterViewport::ResetRuntimeParameters(const UDisplayClusterConfigurationViewport* InConfigurationViewport)
{
	// Reset runtim flags from prev frame:
	RenderSettings.BeginUpdateSettings();
	RenderSettingsICVFX.BeginUpdateSettings();
	PostRenderSettings.BeginUpdateSettings();
	VisibilitySettings.BeginUpdateSettings();
	CameraMotionBlur.BeginUpdateSettings();
	CameraDepthOfField.BeginUpdateSettings();

	OverscanRuntimeSettings = FDisplayClusterViewport_OverscanRuntimeSettings();
	CustomFrustumRuntimeSettings = FDisplayClusterViewport_CustomFrustumRuntimeSettings();

	// Obtain viewport media state from external multicast delegates (This viewport can be used by multiple media).
	EDisplayClusterViewportMediaState AllMediaStates = EDisplayClusterViewportMediaState::None;
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterUpdateViewportMediaState().Broadcast(this, AllMediaStates);

	// Update the media state for the new frame.
	RenderSettings.AssignMediaStates(AllMediaStates);

	// Read general settings from the configuration
	if (InConfigurationViewport)
	{
		if (const FDisplayClusterConfigurationICVFX_StageSettings* StageSettings = Configuration->GetStageSettings())
		{
			// Update base ICVFX settings of viewport.
			RenderSettingsICVFX.Flags = InConfigurationViewport->GetViewportICVFXFlags(*StageSettings);
		}
	}
}

bool FDisplayClusterViewport::IsInternalViewport() const
{
	// Ignore ICVFX internal resources.
	if (EnumHasAnyFlags(GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource))
	{
		return true;
	}

	// Ignore internal tile viewports.
	switch (GetRenderSettings().TileSettings.GetType())
	{
	case EDisplayClusterViewportTileType::None:
	case EDisplayClusterViewportTileType::Source:
		break;

	default:
		return true;
	}

	return false;
}

bool FDisplayClusterViewport::IsExternalRendering() const
{
	if (PostRenderSettings.Replace.IsEnabled())
	{
		// The viewport is replaced by an external texture.
		return true;
	}

	if (RenderSettings.IsViewportOverridden())
	{
		// Viewport texture is overridden from another viewport.
		return true;
	}

	// UV LightCard viewport use unique whole-cluster texture from LC manager
	if (EnumHasAllFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
	{
		// Use external texture from LightCardManager instead of rendering
		return true;
	}

	return false;
}

bool FDisplayClusterViewport::IsRenderEnabled() const
{
	if (IsExternalRendering())
	{
		// The viewport uses an external rendering that overrides its RTT.
		// Disable rendering for this viewport.
		return false;
	}

	if (RenderSettings.bSkipRendering)
	{
		// Skip rendering.
		// For example this feature is used when the ICVFX camera uses full-frame chromakey colour,
		// thereby eliminating the ICVFX camera's viewport rendering for optimization.
		return false;
	}

	// Handle tile rendering rules.
	switch(RenderSettings.TileSettings.GetType())
	{
	case EDisplayClusterViewportTileType::Tile:
	case EDisplayClusterViewportTileType::None:
		// When tile rendering is used, only tiles are rendered.
		break;

	default:
		// When using tile rendering, other viewport types should never be rendered.
		return false;
	}

	// Handle media rendering rules.
	if (!IsRenderEnabledByMedia())
	{
		// rendering of this viewport is not allowed by media.
		return false;
	}

	return true;
}

bool FDisplayClusterViewport::IsUsedByMedia() const
{
	return RenderSettings.HasAnyMediaStates(EDisplayClusterViewportMediaState::Input | EDisplayClusterViewportMediaState::Capture);
}

bool FDisplayClusterViewport::ShouldApplyMaxTextureConstraints() const
{
	if (IsUsedByMedia())
	{
		// Do not apply this restriction to a viewport that uses media.
		return false;
	}

	return true;
}

bool FDisplayClusterViewport::IsRenderEnabledByMedia() const
{
	// Use this function to disable rendering of this viewport.
	// All logic for handling media states should reside in this and IsUsedByMedia() functions only.

	static const bool bIsCluster = (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Cluster);
	static const bool bIsOffscreen = FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen"));

	const bool bHasInputAssigned = RenderSettings.HasAnyMediaStates(EDisplayClusterViewportMediaState::Input);
	const bool bHasOutputAssigned = RenderSettings.HasAnyMediaStates(EDisplayClusterViewportMediaState::Capture);

	// Do not render the viewport if it is used as a media input.
	if (bHasInputAssigned)
	{
		// This viewport is not rendered but gets the image from the media.
		// (media input replaces rendering.)
		return false;
	}

	// Applying special media rules for tiles for cluster modes only.
	if (bIsCluster && !bHasOutputAssigned
		&& RenderSettings.TileSettings.GetType() == EDisplayClusterViewportTileType::Tile)
	{
		// Tiles logic is a little more complicated
		//
		// In | Out | Render (offscreen) | Render (onscreen)
		// ------------------------------------------------------------------
		//  0 |  0  |        0           | bShouldRenderUnbound
		//  0 |  1  |        1           | 1
		//  1 |  0  |        0           | 0
		//  1 |  1  |        0           | 0
		// Check if current node was explicitly allowed to render unbound tiles
		const bool bShouldRenderUnbound = RenderSettings.TileSettings.HasAnyTileFlags(EDisplayClusterViewportTileFlags::AllowUnboundRender);
		const bool bShouldRender = !bIsOffscreen && bShouldRenderUnbound;
		if (!bShouldRender)
		{
			// Disable tile viewport rendering.
			return false;
		}
	}

	return true;
}

bool FDisplayClusterViewport::CanSplitIntoTiles() const
{
	if (!RenderSettings.bEnable || RenderSettings.bSkipRendering || RenderSettings.bFreezeRendering)
	{
		// When this viewport is not rendering, ignore tile splitting.
		return false;
	}

	// Ignore internal tile viewports.
	switch(RenderSettings.TileSettings.GetType())
	{
		case EDisplayClusterViewportTileType::Tile:
		case EDisplayClusterViewportTileType::UnusedTile:
			return false;

		default:
			break;
	}

	// Ignore viewports that have a link to another.
	if (RenderSettings.IsViewportHasParent())
	{
		return false;
	}

	// Ignore viewports that use an external source instead of rendering.
	if (IsExternalRendering())
	{
		return false;
	}

	// Ignore viewports that used by media
	if (IsUsedByMedia())
	{
		return false;
	}

	return true;
}

bool FDisplayClusterViewport::IsResourceUsedImpl(const EDisplayClusterViewportResource InResourceType) const
{
	if (RenderSettings.bSkipRendering)
	{
		// When rendering is skipped, the resources isn't used
		// For example this feature is used when the ICVFX camera uses full-frame chromakey colour,
		// thereby eliminating the ICVFX camera's viewport rendering resources for optimization.
		return false;
	}

	if (RenderSettings.TileSettings.GetType() == EDisplayClusterViewportTileType::Tile)
	{
		// The tiled viewport does not use internal resources.
		// Since rendering results are copied between RTTs.
		if (InResourceType != EDisplayClusterViewportResource::RenderTargets)
		{
			return false;
		}
	}

	if (InResourceType == EDisplayClusterViewportResource::RenderTargets)
	{
		// When external rendering is used, this means that the RTT is a reference, not a texture.
		if (IsExternalRendering())
		{
			return false;
		}
	}

	if (RenderSettings.IsViewportOverridden())
	{
		switch (RenderSettings.GetViewportOverrideMode())
		{
		// Override all resources
		case EDisplayClusterViewportOverrideMode::All:
			switch (InResourceType)
			{
			case EDisplayClusterViewportResource::RenderTargets:
			case EDisplayClusterViewportResource::InputShaderResources:
			case EDisplayClusterViewportResource::MipsShaderResources:
			case EDisplayClusterViewportResource::AdditionalTargetableResources:
				return false;

			default:
				break;
			}
			break;

		// Override all internal resources except the texture for warpblend
		case EDisplayClusterViewportOverrideMode::InternalViewportResources:
			switch (InResourceType)
			{
			case EDisplayClusterViewportResource::RenderTargets:
			case EDisplayClusterViewportResource::InputShaderResources:
			case EDisplayClusterViewportResource::MipsShaderResources:
				return false;

			default:
				break;
			}
			break;

		case EDisplayClusterViewportOverrideMode::InternalRTT:
			// When all resources in a viewport are overridden from another viewport.
			if (InResourceType == EDisplayClusterViewportResource::RenderTargets)
			{
				return false;
			}
			break;

		default:
			break;
		}
	}

	// These are exceptions to the rules, for each case we must return true or false:
	switch (InResourceType)
	{
	case EDisplayClusterViewportResource::AdditionalTargetableResources:
		// Supoport projection policy additional resource
		if (ProjectionPolicy.IsValid() && ProjectionPolicy->ShouldUseAdditionalTargetableResource(this))
		{
			return true;
		}

		return false;

		// Return true if this viewport requires to use any of output resources (OutputPreviewTargetableResources or OutputFrameTargetableResources)
	case EDisplayClusterViewportResource::OutputPreviewTargetableResources:
	case EDisplayClusterViewportResource::OutputFrameTargetableResources:
		// Only if this viewport is enabled and visible on the final frame.
		return RenderSettings.bEnable && RenderSettings.bVisible;

	default:
		break;
	}

	return true;
}

bool FDisplayClusterViewport::IsResourceUsed(const EDisplayClusterViewportResource InResourceType) const
{
	switch (InResourceType)
	{
	case EDisplayClusterViewportResource::AdditionalFrameTargetableResources:
		// OutputFrameTargetableResources must be used for AdditionalFrameTargetableResource
		if (IsResourceUsedImpl(EDisplayClusterViewportResource::OutputFrameTargetableResources))
		{
			// viewport remap requires AdditionalFrameTargetableResources.
			// should be reviewed in the future.
			if (ViewportRemap.IsUsed())
			{
				return true;
			}
		}
		return false;

	default:
		break;
	}

	return IsResourceUsedImpl(InResourceType);
};

bool FDisplayClusterViewport::ShouldUseFullSizeFrameTargetableResource() const
{
	if (ViewportRemap.IsUsed())
	{
		return true;
	}

	return false;
}

bool FDisplayClusterViewport::ShouldFreezeRender() const
{
	// Freeze preview rendering for all viewport
	if (Configuration->GetRenderFrameSettings().IsPreviewFreezeRender())
	{
		return true;
	}

	// ICVFX: Freeze only some viewports
	if (const FDisplayClusterConfigurationICVFX_StageSettings* StageSettings = Configuration->GetStageSettings())
	{	
		if (StageSettings->bFreezeRenderOuterViewports)
		{
			// Freeze rendering for outer viewports
			if (!IsInternalViewport())
			{
				return true;
			}

			// Enable\Disable freeze rendering for lightcards when outer viewports rendering also freezed. This will impact performance.
			if (!StageSettings->Lightcard.bIgnoreOuterViewportsFreezingForLightcards
				&& EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
			{
				return true;
			}
		}
	}
	
	return false;
}

bool FDisplayClusterViewport::ShouldUseViewStates() const
{
	// RenderMode rules
	switch (Configuration->GetRenderFrameSettings().RenderMode)
	{
		// Cluster node rendering
	case EDisplayClusterRenderFrameMode::Mono:
	case EDisplayClusterRenderFrameMode::Stereo:
	case EDisplayClusterRenderFrameMode::SideBySide:
	case EDisplayClusterRenderFrameMode::TopBottom:
		// These modes use ViewState from ULocalPLayer, outside of nDisplay.
		return false;

		// MRQ rendering
	case EDisplayClusterRenderFrameMode::MRQ_Mono:
		// These modes use ViewState from MRQ, outside of nDisplay.
		return false;

	// Preview-In-Scene
	case EDisplayClusterRenderFrameMode::PreviewProxyHitInScene:
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		if (GDisplayClusterPreviewEnableViewState == 0)
		{
			return false;
		}

		// Disable ViewState for the preview inside the Configurator.
		if (GDisplayClusterPreviewEnableConfiguratorViewState == 0 && Configuration->IsCurrentWorldHasAnyType(EWorldType::EditorPreview))
		{
			return false;
		}

		break;

		// PIE
	case EDisplayClusterRenderFrameMode::PIE_Mono:
	case EDisplayClusterRenderFrameMode::PIE_SideBySide:
	case EDisplayClusterRenderFrameMode::PIE_TopBottom:
		// We don't have any rules for PIE rendering
		break;


	default:
		return false;
	}

	// Viewport type rules
	switch (RenderSettings.CaptureMode)
	{
	case EDisplayClusterViewportCaptureMode::Chromakey:
		return false;

	default:
		break;
	}

	return true;
}

bool FDisplayClusterViewport::IsOpenColorIOEquals(const FDisplayClusterViewport& InViewport) const
{
	bool bEnabledOCIO_1 = OpenColorIO.IsValid();
	bool bEnabledOCIO_2 = InViewport.OpenColorIO.IsValid();

	if (bEnabledOCIO_1 == bEnabledOCIO_2)
	{
		if (!bEnabledOCIO_1)
		{
			// Both OCIO disabled
			return true;
		}

		if (OpenColorIO->IsConversionSettingsEqual(InViewport.OpenColorIO->GetConversionSettings()))
		{
			return true;
		}
	}

	return false;
}

/**
* The viewport priority values
* A lower priority value for a viewport means that this viewport will be the first in the list of viewports.
* The order in this list is used to process viewports one after the other.
* This is important when the viewports are linked to each other.
*/
enum class EDisplayClusterViewportPriority : uint8
{
	None = 0,

	// This viewport does not use tile rendering.
	TileDisable = (1 << 0),

	// This tile source viewport should be configured before tiles.
	TileSource = (1 << 0),

	// The tile viewport setup after the tile.
	Tile = (1 << 1),

	// The linked viewport should be right after the parent viewports because it uses data from them.
	Linked = (1 << 2),

	// Overridden viewports are not rendered but depend on their source anyway, so they are processed at the end.
	Overriden = (1 << 3),
};

uint8 FDisplayClusterViewport::GetPriority() const
{
	uint8 OutOrder = 0;

	// Tile rendering requires a special viewport processing order for the game thread.
	switch (RenderSettings.TileSettings.GetType())
	{
	case EDisplayClusterViewportTileType::Source:
		OutOrder += (uint8)EDisplayClusterViewportPriority::TileSource;
		break;
	case EDisplayClusterViewportTileType::Tile:
	case EDisplayClusterViewportTileType::UnusedTile:
		OutOrder += (uint8)EDisplayClusterViewportPriority::Tile;
		break;

	case EDisplayClusterViewportTileType::None:
		OutOrder += (uint8)EDisplayClusterViewportPriority::TileDisable;
		break;

	default:
		break;
	}

	if (RenderSettings.IsViewportHasParent())
	{
		OutOrder += (uint8)EDisplayClusterViewportPriority::Linked;
	}

	if (RenderSettings.IsViewportOverridden())
	{
		OutOrder += (uint8)EDisplayClusterViewportPriority::Overriden;
	}

	return OutOrder;
}

bool FDisplayClusterViewport::ShouldUseLumenPerView() const
{
	switch (RenderSettings.CaptureMode)
	{
	case EDisplayClusterViewportCaptureMode::Chromakey:
	case EDisplayClusterViewportCaptureMode::Lightcard:
		// This type of viewport does not use Lumen
		return false;

	default:
		break;
	}

	const EDisplayClusterLumenPerView LumenPerViewMode = (EDisplayClusterLumenPerView)FMath::Clamp(
		GDisplayClusterLumenPerView, 0, (uint8)EDisplayClusterLumenPerView::MAX);

	switch (LumenPerViewMode)
	{
	// Use only in Cluster.
	case EDisplayClusterLumenPerView::UseOnlyInCluster:
		return !Configuration->IsPreviewRendering();

	// Use only in Preview.
	case EDisplayClusterLumenPerView::UseOnlyInPreview:
		return Configuration->IsPreviewRendering();

	// Use only for Inner Frustum in Cluster.
	case EDisplayClusterLumenPerView::UseOnlyForICVXFCameraInCluster:
		if (Configuration->IsPreviewRendering())
		{
			// Not a cluster
			return false;
		}

		// Full-frame In-Camera viewport
		if (EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InCamera))
		{
			return true;
		}

		// Tile In-Camera viewport
		if (RenderSettings.TileSettings.GetType() == EDisplayClusterViewportTileType::Tile)
		{
			if (FDisplayClusterViewportManager* ViewportManager = Configuration->GetViewportManagerImpl())
			{
				TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> SourceViewport =
					ViewportManager->ImplFindViewport(RenderSettings.TileSettings.GetSourceViewportId());

				if (SourceViewport.IsValid())
				{
					if (EnumHasAnyFlags(SourceViewport->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InCamera))
					{
						return true;
					}
				}
			}
		}
		
		// Other viewports
		return false;

	// Use in both Cluster and Preview.
	case EDisplayClusterLumenPerView::UseEverywhere:
		return true;

	default:
		break;
	}

	return false;
}
