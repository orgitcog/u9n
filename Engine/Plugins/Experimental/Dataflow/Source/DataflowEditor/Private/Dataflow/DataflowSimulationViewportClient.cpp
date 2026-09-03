// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowSimulationViewportClient.h"

#include "AssetEditorModeManager.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorOptions.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorCollectionComponent.h"
#include "Dataflow/DataflowEngineSceneHitProxies.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowEditorPreviewSceneBase.h"
#include "Dataflow/DataflowSimulationScene.h"
#include "Dataflow/DataflowSimulationVisualization.h"
#include "EditorModeManager.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "GraphEditor.h"
#include "PreviewScene.h"
#include "Selection.h"
#include "SGraphPanel.h"
#include "SNodePanel.h"

FDataflowSimulationViewportClient::FDataflowSimulationViewportClient(FEditorModeTools* InModeTools,
                                                             FPreviewScene* InPreviewScene,  const bool bCouldTickScene,
                                                             const TWeakPtr<SEditorViewport> InEditorViewportWidget)
	: FDataflowEditorViewportClientBase(InModeTools, InPreviewScene, bCouldTickScene, InEditorViewportWidget)
{
	// We want our near clip plane to be quite close so that we can zoom in further.
	OverrideNearClipPlane(KINDA_SMALL_NUMBER);

	EngineShowFlags.SetSelectionOutline(true);
	EngineShowFlags.EnableAdvancedFeatures();

	bEnableSceneTicking = bCouldTickScene;

	if (UDataflowEditorOptions* const Options = UDataflowEditorOptions::StaticClass()->GetDefaultObject<UDataflowEditorOptions>())
	{
		FOVAngle = Options->SimulationViewFOV;
		ViewFOV = FOVAngle;
		ExposureSettings.bFixed = Options->bSimulationViewFixedExposure;
	}
}

FDataflowSimulationViewportClient::~FDataflowSimulationViewportClient()
{
	if (UDataflowEditorOptions* const Options = UDataflowEditorOptions::StaticClass()->GetDefaultObject<UDataflowEditorOptions>())
	{
		Options->SimulationViewFOV = FOVAngle;
		Options->bSimulationViewFixedExposure = ExposureSettings.bFixed;
		Options->SaveConfig();
	}
}

void FDataflowSimulationViewportClient::SetDataflowEditorToolkit(TWeakPtr<FDataflowEditorToolkit> InDataflowEditorToolkitPtr)
{
	DataflowEditorToolkitPtr = InDataflowEditorToolkitPtr;
}

void FDataflowSimulationViewportClient::SetToolCommandList(TWeakPtr<FUICommandList> InToolCommandList)
{
	ToolCommandList = InToolCommandList;
}

void FDataflowSimulationViewportClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DataflowPreviewScene)
	{
		DataflowPreviewScene->TickDataflowScene(DeltaSeconds);
	}
}

void FDataflowSimulationViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	Super::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
	OnViewportClicked(HitProxy);
}

void FDataflowSimulationViewportClient::OnViewportClicked(HHitProxy* HitProxy)
{
	USelection* SelectedComponents = ModeTools->GetSelectedComponents();
	
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

	TArray<UPrimitiveComponent*> CurrentlySelectedComponents;
	SelectedComponents->GetSelectedObjects<UPrimitiveComponent>(CurrentlySelectedComponents);

	// Get all the scene selected elements 
	TArray<FDataflowBaseElement*> DataflowElements;
	GetSelectedElements(HitProxy, DataflowElements);
	
	OnSelectionChangedMulticast.Broadcast(CurrentlySelectedComponents, DataflowElements);
}


void FDataflowSimulationViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FDataflowEditorViewportClientBase::Draw(View, PDI);

	using namespace UE::Dataflow;
	const TMap<FName, TUniquePtr<IDataflowSimulationVisualization>>& Visualizations = FDataflowSimulationVisualizationRegistry::GetInstance().GetVisualizations();
	for (const TPair<FName, TUniquePtr<IDataflowSimulationVisualization>>& Visualization : Visualizations)
	{
		if (PreviewScene)
		{
			Visualization.Value->Draw(static_cast<FDataflowSimulationScene*>(PreviewScene), PDI);
		}
	}
}

void FDataflowSimulationViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);

	using namespace UE::Dataflow;
	const TMap<FName, TUniquePtr<IDataflowSimulationVisualization>>& Visualizations = FDataflowSimulationVisualizationRegistry::GetInstance().GetVisualizations();
	for (const TPair<FName, TUniquePtr<IDataflowSimulationVisualization>>& Visualization : Visualizations)
	{
		if (PreviewScene)
		{
			Visualization.Value->DrawCanvas(static_cast<FDataflowSimulationScene*>(PreviewScene), &Canvas, &View);
		}
	}
}
