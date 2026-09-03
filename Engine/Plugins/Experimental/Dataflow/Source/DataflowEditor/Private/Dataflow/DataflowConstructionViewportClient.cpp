// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowConstructionViewportClient.h"

#include "AssetEditorModeManager.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorOptions.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorCollectionComponent.h"
#include "Dataflow/DataflowEngineSceneHitProxies.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowConstructionScene.h"
#include "Dataflow/DataflowConstructionVisualization.h"
#include "Dataflow/DataflowEditorPreviewSceneBase.h"
#include "Dataflow/DataflowRenderingViewMode.h"
#include "EditorModeManager.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "GraphEditor.h"
#include "PreviewScene.h"
#include "Selection.h"
#include "SGraphPanel.h"
#include "SNodePanel.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "Behaviors/2DViewportBehaviorTargets.h"

#define LOCTEXT_NAMESPACE "DataflowConstructionViewportClient"

FDataflowConstructionViewportClient::FDataflowConstructionViewportClient(FEditorModeTools* InModeTools,
                                                             FPreviewScene* InPreviewScene,  const bool bCouldTickScene,
                                                             const TWeakPtr<SEditorViewport> InEditorViewportWidget)
	: FDataflowEditorViewportClientBase(InModeTools, InPreviewScene, bCouldTickScene, InEditorViewportWidget)
{
	// We want our near clip plane to be quite close so that we can zoom in further.
	OverrideNearClipPlane(KINDA_SMALL_NUMBER);

	// Allow focusing on small objects
	MinimumFocusRadius = 0.1f;

	EngineShowFlags.SetSelectionOutline(true);
	EngineShowFlags.EnableAdvancedFeatures();

	PreviewScene = static_cast<FDataflowPreviewSceneBase*>(InPreviewScene);
	bEnableSceneTicking = bCouldTickScene;

	bool bUseRightMouseButton = true;
	bool bUseMiddleMouseButton = true;
	if (const UDataflowEditorOptions* const Options = GetDefault<UDataflowEditorOptions>())
	{
		bUseRightMouseButton = (Options->ConstructionViewportMousePanButton == EDataflowConstructionViewportMousePanButton::Right || Options->ConstructionViewportMousePanButton == EDataflowConstructionViewportMousePanButton::RightOrMiddle);
		bUseMiddleMouseButton = (Options->ConstructionViewportMousePanButton == EDataflowConstructionViewportMousePanButton::Middle || Options->ConstructionViewportMousePanButton == EDataflowConstructionViewportMousePanButton::RightOrMiddle);
	}

	if (!bUseRightMouseButton)
	{
		// Intercept right mouse drag with a do-nothing ClickDragInputBehavior
		const TObjectPtr<ULocalClickDragInputBehavior> RightMouseClickDragBehavior = NewObject<ULocalClickDragInputBehavior>();
		RightMouseClickDragBehavior->Initialize();

		// We'll have the priority of our viewport behaviors be lower (i.e. higher numerically) than both the gizmo default and the tool default
		constexpr int ViewportBehaviorPriority = FMath::Max(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY, FInputCapturePriority::DEFAULT_TOOL_PRIORITY) + 2;
		RightMouseClickDragBehavior->SetDefaultPriority(ViewportBehaviorPriority);
		RightMouseClickDragBehavior->SetUseRightMouseButton();
		RightMouseClickDragBehavior->CanBeginClickDragFunc = [](const FInputDeviceRay& InputDeviceRay)
		{
			return FInputRayHit(TNumericLimits<float>::Max()); // bHit is true. Depth is max to lose the standard tiebreaker.
		};
		BehaviorsFor2DMode.Add(RightMouseClickDragBehavior);
	}

	if (bUseMiddleMouseButton)
	{
		OrthoScrollBehaviorTarget = MakeUnique<FEditor2DScrollBehaviorTarget>(this);
		UClickDragInputBehavior* const MiddleMouseClickDragInputBehavior = NewObject<UClickDragInputBehavior>();
		MiddleMouseClickDragInputBehavior->Initialize(OrthoScrollBehaviorTarget.Get());

		// We'll have the priority of our viewport behaviors be lower (i.e. higher numerically) than both the gizmo default and the tool default
		constexpr int ViewportBehaviorPriority = FMath::Max(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY, FInputCapturePriority::DEFAULT_TOOL_PRIORITY) + 3;
		MiddleMouseClickDragInputBehavior->SetDefaultPriority(ViewportBehaviorPriority);
		MiddleMouseClickDragInputBehavior->SetUseMiddleMouseButton();
		BehaviorsFor2DMode.Add(MiddleMouseClickDragInputBehavior);
	}

	if (const UDataflowEditorOptions* const Options = UDataflowEditorOptions::StaticClass()->GetDefaultObject<UDataflowEditorOptions>())
	{
		FOVAngle = Options->ConstructionViewFOV;
		ViewFOV = FOVAngle;
		ExposureSettings.bFixed = Options->bConstructionViewFixedExposure;
	}
}

FDataflowConstructionViewportClient::~FDataflowConstructionViewportClient()
{
	if (UDataflowEditorOptions* const Options = UDataflowEditorOptions::StaticClass()->GetDefaultObject<UDataflowEditorOptions>())
	{
		Options->ConstructionViewFOV = FOVAngle;
		Options->bConstructionViewFixedExposure = ExposureSettings.bFixed;
		Options->SaveConfig();
	}
}

void FDataflowConstructionViewportClient::SetDataflowEditorToolkit(TWeakPtr<FDataflowEditorToolkit> InDataflowEditorToolkitPtr)
{
	DataflowEditorToolkitPtr = InDataflowEditorToolkitPtr;
}

void FDataflowConstructionViewportClient::SetToolCommandList(TWeakPtr<FUICommandList> InToolCommandList)
{
	ToolCommandList = InToolCommandList;
}

void FDataflowConstructionViewportClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (PreviewScene)
	{
		PreviewScene->TickDataflowScene(DeltaSeconds);
	}
}

USelection* FDataflowConstructionViewportClient::GetSelectedComponents() 
{
	USelection* SceneSelection = FDataflowPreviewSceneBase::GetSelectedComponents(PreviewScene);
	return SceneSelection ? SceneSelection : ModeTools->GetSelectedComponents(); 
}

bool FDataflowConstructionViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	// See if any tool commands want to handle the key event
	const TSharedPtr<FUICommandList> PinnedToolCommandList = ToolCommandList.Pin();
	if (EventArgs.Event != IE_Released && PinnedToolCommandList.IsValid())
	{
		const FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
		if (PinnedToolCommandList->ProcessCommandBindings(EventArgs.Key, KeyState, (EventArgs.Event == IE_Repeat)))
		{
			return true;
		}
	}

	return FEditorViewportClient::InputKey(EventArgs);
}


void FDataflowConstructionViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	Super::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
	OnViewportClicked(HitProxy);
}

void FDataflowConstructionViewportClient::OnViewportClicked(HHitProxy* HitProxy)
{
	auto EnableToolForSelectedNode = [&](USelection* SelectedComponents)
	{
		if (TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin())
		{
			if (PreviewScene && PreviewScene->GetDataflowModeManager())
			{
				if (UDataflowEditorMode* DataflowMode = Cast<UDataflowEditorMode>(PreviewScene->GetDataflowModeManager()->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
				{
					if (TSharedPtr<SDataflowGraphEditor> GraphEditor = DataflowEditorToolkit->GetDataflowGraphEditor())
					{
						if (UEdGraphNode* SelectedNode = GraphEditor->GetSingleSelectedNode())
						{
							if (SelectedComponents && SelectedComponents->Num() == 1)
							{
								DataflowMode->StartToolForSelectedNode(SelectedNode);
							}
						}
					}
				}
			}
		}
	};

	auto UpdateSelectedComponentInViewport = [&](USelection* SelectedComponents)
	{
		TArray<UPrimitiveComponent*> PreviouslySelectedComponents;
		SelectedComponents->GetSelectedObjects<UPrimitiveComponent>(PreviouslySelectedComponents);

		SelectedComponents->Modify();
		SelectedComponents->BeginBatchSelectOperation();

		SelectedComponents->DeselectAll();

		if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
		{
			const HActor* ActorProxy = static_cast<HActor*>(HitProxy);
			if (ActorProxy && ActorProxy->PrimComponent && ActorProxy->Actor)
			{
				UPrimitiveComponent* Component = const_cast<UPrimitiveComponent*>(ActorProxy->PrimComponent.Get());
				SelectedComponents->Select(Component);
				Component->PushSelectionToProxy();
			}
		}

		SelectedComponents->EndBatchSelectOperation();

		for (UPrimitiveComponent* const Component : PreviouslySelectedComponents)
		{
			Component->PushSelectionToProxy();
		}
	};

	auto SelectSingleNodeInGraph = [&](TObjectPtr<const UDataflowEdNode> Node)
	{
		if (TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin())
		{
			if (TSharedPtr<SDataflowGraphEditor> GraphEditor = DataflowEditorToolkit->GetDataflowGraphEditor())
			{
				GraphEditor->GetGraphPanel()->SelectionManager.SelectSingleNode((UObject*)Node.Get());
			}
		}
	};
	
	auto IsInteractiveToolActive = [&]()
	{
		if (UDataflowEditorMode* DataflowMode = Cast<UDataflowEditorMode>(
			PreviewScene->GetDataflowModeManager()->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
		{
			if (UEditorInteractiveToolsContext* const ToolsContext = DataflowMode->GetInteractiveToolsContext())
			{
				return ToolsContext->HasActiveTool();
			}
		}
		return false;
	};

	TArray<UPrimitiveComponent*> CurrentlySelectedComponents;
	if (!IsInteractiveToolActive())
	{
		if (USelection* SelectedComponents = GetSelectedComponents())
		{
			UpdateSelectedComponentInViewport(SelectedComponents);

			if (bool bIsAltKeyDown = Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt))
			{
				if (UDataflowEditorCollectionComponent* DataflowComponent
					= SelectedComponents->GetBottom<UDataflowEditorCollectionComponent>())
				{
					SelectSingleNodeInGraph(DataflowComponent->Node);
				}
			}

			EnableToolForSelectedNode(SelectedComponents);

			SelectedComponents->GetSelectedObjects<UPrimitiveComponent>(CurrentlySelectedComponents);
		}
	}
	
	// Get all the scene selected elements 
	TArray<FDataflowBaseElement*> DataflowElements;
	GetSelectedElements(HitProxy, DataflowElements);
	
	OnSelectionChangedMulticast.Broadcast(CurrentlySelectedComponents, DataflowElements);
}

void FDataflowConstructionViewportClient::SetConstructionViewMode(const UE::Dataflow::IDataflowConstructionViewMode* InViewMode)
{
	checkf(InViewMode, TEXT("SetConstructionViewMode received null IDataflowConstructionViewMode pointer"));

	if (ConstructionViewMode)
	{
		SavedInactiveViewTransforms.FindOrAdd(ConstructionViewMode->GetName()) = GetViewTransform();
	}

	ConstructionViewMode = InViewMode;

	//
	// Update input behaviors
	//

	BehaviorSet->RemoveAll();

	for (UInputBehavior* const Behavior : BaseBehaviors)
	{
		BehaviorSet->Add(Behavior);
	}

	if (!InViewMode->IsPerspective())
	{
		for (UInputBehavior* const Behavior : BehaviorsFor2DMode)
		{
			BehaviorSet->Add(Behavior);
		}
	}

	ModeTools->GetInteractiveToolsContext()->InputRouter->DeregisterSource(this);
	ModeTools->GetInteractiveToolsContext()->InputRouter->RegisterSource(this);


	SetViewportType((ELevelViewportType)ConstructionViewMode->GetViewportType());

	if (const FViewportCameraTransform* const FoundPreviousTransform = SavedInactiveViewTransforms.Find(InViewMode->GetName()))
	{
		if (ConstructionViewMode->IsPerspective())
		{
			ViewTransformPerspective = *FoundPreviousTransform;
		}
		else
		{
			ViewTransformOrthographic = *FoundPreviousTransform;
		}
	}
	else
	{
		// TODO: Default view transform
	}

	bDrawAxes = ConstructionViewMode->IsPerspective();
	Invalidate();
}


void FDataflowConstructionViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FDataflowEditorViewportClientBase::Draw(View, PDI);

	using namespace UE::Dataflow;
	for (const TPair<FName, TUniquePtr<IDataflowConstructionVisualization>>& Visualization : FDataflowConstructionVisualizationRegistry::GetInstance().GetVisualizations())
	{
		Visualization.Value->Draw(static_cast<FDataflowConstructionScene*>(PreviewScene), PDI, View);
	}
}

void FDataflowConstructionViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);

	using namespace UE::Dataflow;
	for (const TPair<FName, TUniquePtr<IDataflowConstructionVisualization>>& Visualization : FDataflowConstructionVisualizationRegistry::GetInstance().GetVisualizations())
	{
		Visualization.Value->DrawCanvas(static_cast<FDataflowConstructionScene*>(PreviewScene), &Canvas, &View);
	}
}

float FDataflowConstructionViewportClient::GetMinimumOrthoZoom() const
{
	// Ignore ULevelEditorViewportSettings::MinimumOrthographicZoom in this viewport client
	return 1.0f;
}

void FDataflowConstructionViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	FDataflowEditorViewportClientBase::AddReferencedObjects(Collector);
	Collector.AddReferencedObjects(BehaviorsFor2DMode);
}

FString FDataflowConstructionViewportClient::GetOverlayString() const
{
	if (TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin())
	{
		return DataflowEditorToolkit->GetDebugDrawOverlayString();
	}

	return {};
}

#undef LOCTEXT_NAMESPACE 
