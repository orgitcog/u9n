// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Preview/DisplayClusterViewportManagerPreview.h"
#include "Render/Viewport/Preview/DisplayClusterViewportPreview.h"
#include "Render/Viewport/Preview/DisplayClusterViewportManagerPreviewRendering.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "DisplayClusterRootActor.h"
#include "DisplayClusterConfigurationTypes.h"

#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"

#include "EngineModule.h"
#include "CanvasTypes.h"
#include "LegacyScreenPercentageDriver.h"

#include "SceneView.h"
#include "SceneViewExtension.h"

#include "Engine/Scene.h"
#include "GameFramework/WorldSettings.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewport_CustomPostProcessSettings.h"
#include "Render/Viewport/Resource/DisplayClusterViewportResource.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterRootActor.h" 

#include "ClearQuad.h"

////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportManagerPreview
////////////////////////////////////////////////////////////////////////////////////////
int32 FDisplayClusterViewportManagerPreview::RenderClusterNodePreview(const int32 InViewportsAmmount, FViewport* InViewport, FCanvas* InSceneCanvas)
{
	// InViewportsAmmount - Total ammount of viewports that should be rendered on this frame.
	// INDEX_NONE means render all viewports of this node
	int32 OutViewportsAmount = (InViewportsAmmount == INDEX_NONE) ? (ViewportsViewFamily.Num() + 1) : InViewportsAmmount;

	UWorld* CurrentWorld = Configuration->GetCurrentWorld();
	FDisplayClusterViewportManager* ViewportManager = Configuration->GetViewportManagerImpl();
	if (PreviewRenderFrame.IsValid() && OutViewportsAmount > 0 && CurrentWorld && ViewportManager)
	{

		// Render all viewports
		while (!ViewportsViewFamily.IsEmpty() && OutViewportsAmount > 0)
		{
			FSceneViewFamily& ViewFamily = *ViewportsViewFamily[0];

			if (InSceneCanvas)
			{
				GetRendererModule().BeginRenderingViewFamily(InSceneCanvas, &ViewFamily);
			}
			else
			{
				const ERHIFeatureLevel::Type FeatureLevel = CurrentWorld ? CurrentWorld->GetFeatureLevel() : GMaxRHIFeatureLevel;
				FCanvas Canvas((FRenderTarget*)ViewportsViewFamily[0]->RenderTarget, nullptr, CurrentWorld, FeatureLevel, FCanvas::CDM_DeferDrawing /*FCanvas::CDM_ImmediateDrawing*/, 1.0f);
				Canvas.Clear(FLinearColor::Black);

				GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);
			}

			if (GNumExplicitGPUsForRendering > 1)
			{
				const FRHIGPUMask SubmitGPUMask = ViewFamily.Views.Num() == 1 ? ViewFamily.Views[0]->GPUMask : FRHIGPUMask::All();
				ENQUEUE_RENDER_COMMAND(UDisplayClusterViewportClient_SubmitCommandList)(
					[SubmitGPUMask](FRHICommandListImmediate& RHICmdList)
					{
						SCOPED_GPU_MASK(RHICmdList, SubmitGPUMask);
						RHICmdList.SubmitCommandsHint();
					});
			}

			ViewportsViewFamily.RemoveAt(0);
			OutViewportsAmount--;

		}

		// After all viewports we render compose
		if (OutViewportsAmount > 0)
		{
			OutViewportsAmount--;

			// Handle special viewports game-thread logic at frame end
			// custom postprocess single frame flag must be removed at frame end on game thread
			ViewportManager->FinalizeNewFrame();

			// After all render target rendered call nDisplay frame rendering:
			ViewportManager->RenderFrame(InViewport);

			// Current node render is completed
			PreviewRenderFrame.Reset();

			// Send event about cluster node rendering is finished
			OnClusterNodePreviewGenerated.ExecuteIfBound(Configuration->GetClusterNodeId());
		}
	}

	return OutViewportsAmount;
}

bool FDisplayClusterViewportManagerPreview::InitializeClusterNodePreview(const EDisplayClusterRenderFrameMode InRenderMode, UWorld* InWorld, const FString& InClusterNodeId, FViewport* InViewport)
{
	check(InWorld);

	PreviewRenderFrame.Reset();
	ViewportsViewFamily.Empty();

	if (FDisplayClusterViewportManager* ViewportManager = Configuration->GetViewportManagerImpl())
	{
		// Update preview configuration for cluster node
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FDisplayClusterViewportManagerPreview::BeginClusterNodeRendering"), STAT_DisplayClusterViewportManagerPreview_BeginClusterNodeRendering, STATGROUP_NDisplay);

		// Update local node viewports (update\create\delete) and build new render frame
		if (Configuration->UpdateConfigurationForClusterNode(InRenderMode, InWorld, InClusterNodeId))
		{
			// Build cluster node render frame
			PreviewRenderFrame = MakeUnique<FDisplayClusterRenderFrame>();
			if (ViewportManager->BeginNewFrame(InViewport, *PreviewRenderFrame))
			{
				// Initialize frame for render
				ViewportManager->InitializeNewFrame();

				// Create viewfamilies for all viewports
				FSceneInterface* PreviewScene = InWorld->Scene;
				FEngineShowFlags EngineShowFlags = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);

				for (const FDisplayClusterRenderFrameTarget& RenderTargetIt : PreviewRenderFrame->RenderTargets)
				{
					// Special flag, allow clear RTT surface only for first family
					bool bAdditionalViewFamily = false;
					for (const FDisplayClusterRenderFrameTargetViewFamily& ViewFamiliesIt : RenderTargetIt.ViewFamilies)
					{
						// Create the view family for rendering the world scene to the viewport's render target
						TSharedRef<FSceneViewFamilyContext> ViewFamily = MakeShared<FSceneViewFamilyContext>(ViewportManager->CreateViewFamilyConstructionValues(
							RenderTargetIt,
							PreviewScene,
							EngineShowFlags,
							bAdditionalViewFamily
						));

						ViewportManager->ConfigureViewFamily(RenderTargetIt, ViewFamiliesIt, *ViewFamily);

						if (RenderTargetIt.CaptureMode == EDisplayClusterViewportCaptureMode::Default && Configuration->GetRenderFrameSettings().IsPostProcessDisabled())
						{
							if (ViewFamily->EngineShowFlags.TemporalAA)
							{
								ViewFamily->EngineShowFlags.SetTemporalAA(false);
								ViewFamily->EngineShowFlags.SetAntiAliasing(true);
							}
						}

						TArray<FSceneView*> Views;
						for (const FDisplayClusterRenderFrameTargetView& ViewIt : ViewFamiliesIt.Views)
						{
							FDisplayClusterViewport* ViewportPtr = static_cast<FDisplayClusterViewport*>(ViewIt.Viewport.Get());
							if (!ViewportPtr)
							{
								continue;
							}

							check(ViewportPtr->GetContexts().IsValidIndex(ViewIt.ContextNum));

							// Always call CalcSceneView() because this function also starts VE and after that calls FDisplayClusterViewport::SetupSceneView() -> OCIO
							// uvLC viewport is not rendered, but late OCIO is used in FDisplayClusterViewportProxy::ApplyOCIO_RenderThread()
							FSceneView* View = ViewportPtr->ViewportPreview->CalcSceneView(*ViewFamily, ViewIt.ContextNum);

							if (View == nullptr)
							{
								// This viewport is not rendering. Release all textures.
								// The preview mesh materials associated with them will revert to default materials.
								// In this case, all invalid viewports will be displayed gray or black.
								ViewportPtr->ReleaseTextures();
							}

							// Remove the view that is not used for rendering.
							if (View && (!ViewIt.IsViewportContextCanBeRendered() || ViewFamily->RenderTarget == nullptr))
							{
								ViewFamily->Views.Remove(View);

								delete View;
								View = nullptr;
							}

							if (View)
							{
								Views.Add(View);
							}
						}

						if (!ViewFamily->Views.IsEmpty())
						{
							ViewportManager->PostConfigureViewFamily(RenderTargetIt, ViewFamiliesIt, *ViewFamily, Views);
							ViewportsViewFamily.Add(ViewFamily);
						}
					}
				}

				// Update viewport preview instances if preview is used and DCRA supports previews:
				Update();

				return ViewportsViewFamily.Num() > 0;
			}
		}
	}

	PreviewRenderFrame.Reset();
	ViewportsViewFamily.Empty();

	return false;
}
