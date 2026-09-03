// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorToolkit.h"

#include "AdvancedPreviewScene.h"
#include "AdvancedPreviewSceneModule.h"
#include "AssetEditorModeManager.h"
#include "AssetViewerSettings.h"
#include "DetailCategoryBuilder.h"
#include "Animation/Skeleton.h"
#include "Dataflow/DataflowAdvancedPreviewDetailsTab.h"
#include "Dataflow/DataflowAssetViewerSettingsCustomization.h"
#include "Dataflow/DataflowDebugDrawComponent.h"
#include "Dataflow/DataflowDebugDraw.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowCollectionSpreadSheetWidget.h"
#include "Dataflow/DataflowConstructionScene.h"
#include "Dataflow/DataflowConstructionViewportClient.h"
#include "Dataflow/DataflowEditorCollectionComponent.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorModeToolkit.h"
#include "Dataflow/DataflowEditorModule.h"
#include "Dataflow/DataflowEditorModeUILayer.h"
#include "Dataflow/DataflowEditorUtil.h"
#include "Dataflow/DataflowEditorSubGraphTabSummoner.h"
#include "Dataflow/DataflowConstructionViewport.h"
#include "Dataflow/DataflowEditorOptions.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowNodeDetailExtension.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowInstance.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowEditorPreviewSceneBase.h"
#include "Dataflow/DataflowRenderingFactory.h"
#include "Dataflow/DataflowSceneProfileIndexStorage.h"
#include "Dataflow/DataflowSimulationScene.h"
#include "Dataflow/DataflowSimulationVisualization.h"
#include "Dataflow/DataflowSchema.h"
#include "Dataflow/DataflowSkeletonView.h"
#include "Dataflow/DataflowOutlinerView.h"
#include "Dataflow/DataflowSimulationViewportClient.h"
#include "Dataflow/DataflowSubGraphNodes.h"
#include "Dataflow/DataflowMembersWidget.h"
#include "DynamicMeshBuilder.h"
#include "EditorModeManager.h"
#include "EditorStyleSet.h"
#include "EditorViewportTabContent.h"
#include "EditorViewportLayout.h"
#include "EditorViewportCommands.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Commands/GenericCommands.h"
#include "GameFramework/Actor.h"
#include "GraphEditorActions.h"
#include "IDetailCustomization.h"
#include "ISkeletonTree.h"
#include "ISceneOutliner.h"
#include "IStructureDetailsView.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Selection.h"
#include "Styling/SlateStyleRegistry.h"
#include "Widgets/Docking/SDockTab.h"
#include "GeometryCache.h"
#include "ToolMenus.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Dataflow/DataflowOutputLog.h"
#include "Dataflow/DataflowPath.h"
#include "Dataflow/DataflowSimulationNodes.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"

#define LOCTEXT_NAMESPACE "DataflowEditorToolkit"

//DEFINE_LOG_CATEGORY_STATIC(EditorToolkitLog, Log, All);

bool bDataflowEnableSkeletonView = false;
FAutoConsoleVariableRef CVARDataflowEnableSkeletonView(TEXT("p.Dataflow.Editor.EnableSkeletonView"), bDataflowEnableSkeletonView,
	TEXT("Deprecated Tool! Allows the Dataflow editor to create a skeleton view that reflects the hierarchy and selection state of the construction viewport.[def:false]"));

const FName FDataflowEditorToolkit::GraphCanvasTabId(TEXT("DataflowEditor_GraphCanvas"));
const FName FDataflowEditorToolkit::SubGraphCanvasTabId(TEXT("DataflowEditor_SubGraphTab"));
const FName FDataflowEditorToolkit::NodeDetailsTabId(TEXT("DataflowEditor_NodeDetails"));
const FName FDataflowEditorToolkit::PreviewSceneTabId(TEXT("DataflowEditor_PreviewScene"));
const FName FDataflowEditorToolkit::OutlinerViewTabId(TEXT("DataflowEditor_SceneOutliner"));
const FName FDataflowEditorToolkit::SkeletonViewTabId(TEXT("DataflowEditor_SkeletonView"));
const FName FDataflowEditorToolkit::SelectionViewTabId_1(TEXT("DataflowEditor_SelectionView_1"));
const FName FDataflowEditorToolkit::SelectionViewTabId_2(TEXT("DataflowEditor_SelectionView_2"));
const FName FDataflowEditorToolkit::SelectionViewTabId_3(TEXT("DataflowEditor_SelectionView_3"));
const FName FDataflowEditorToolkit::SelectionViewTabId_4(TEXT("DataflowEditor_SelectionView_4"));
const FName FDataflowEditorToolkit::CollectionSpreadSheetTabId_1(TEXT("DataflowEditor_CollectionSpreadSheet_1"));
const FName FDataflowEditorToolkit::CollectionSpreadSheetTabId_2(TEXT("DataflowEditor_CollectionSpreadSheet_2"))
 ;
const FName FDataflowEditorToolkit::CollectionSpreadSheetTabId_3(TEXT("DataflowEditor_CollectionSpreadSheet_3"));
const FName FDataflowEditorToolkit::CollectionSpreadSheetTabId_4(TEXT("DataflowEditor_CollectionSpreadSheet_4"));
const FName FDataflowEditorToolkit::SimulationViewportTabId(TEXT("DataflowEditor_SimulationViewport"));
const FName FDataflowEditorToolkit::SimulationVisualizationTabId(TEXT("DataflowEditor_SimulationVisualizationTab"));
const FName FDataflowEditorToolkit::MembersWidgetTabId(TEXT("DataflowEditor_MembersWidgetTab"));
const FName FDataflowEditorToolkit::OutputLogTabId(TEXT("DataflowEditor_OutputLog"));

namespace UE::Dataflow::Private
{
	static const FName DataflowEditorToolkitName("DataflowEditor");

	/** Create a debug draw component to visualize per node informations */
	TUniquePtr<IDataflowDebugDrawInterface> CreateDebugDrawComponent(
		FDataflowPreviewSceneBase* DataflowScene,
		TObjectPtr<class UDataflowDebugDrawComponent>& DebugDrawComponent)
	{
		if (DataflowScene && !DebugDrawComponent)
		{
			if (TObjectPtr<AActor> RootActor = DataflowScene->GetRootActor())
			{
				if (UDataflowDebugDrawComponent* Component = Cast<UDataflowDebugDrawComponent>(RootActor->FindComponentByClass<UDataflowDebugDrawComponent>()))
				{
					DebugDrawComponent = Cast<UDataflowDebugDrawComponent>(Component);
				}
				else
				{
					DebugDrawComponent = NewObject<UDataflowDebugDrawComponent>(RootActor, FName(LOCTEXT("DataflowDebugDrawComponent", "Dataflow Debug Draw Component").ToString(), RF_Transient));
					DebugDrawComponent->RegisterComponentWithWorld(DataflowScene->GetWorld());
				}
			}
		}

		checkf(DebugDrawComponent, TEXT("Could not create or find a DebugDrawComponentComponent"));

		FDataflowDebugRenderSceneProxy* SceneProxy = static_cast<FDataflowDebugRenderSceneProxy*>(DebugDrawComponent->GetSceneProxy());
		checkf(SceneProxy, TEXT("Could not find a FDataflowDebugRenderSceneProxy on the DebugDrawComponent"));

		return MakeUnique<FDataflowDebugDraw>(SceneProxy, DataflowScene->ModifySceneElements());
	}

	/** Update the debug draw component from the selected/pinned nodes matching the NodeType */
	template<typename NodeType>
	void UpdateDebugDrawComponent(const TObjectPtr<UDataflowBaseContent>& EditorContent,
		const FString& RootName, FDataflowPreviewSceneBase* DataflowScene, const bool bIsConstruction, FString& DebugDrawOverlay,
		const TSharedPtr<FEditorModeTools> EditorModeManager)
	{
		if(!DataflowScene || !EditorContent)
		{
			return;
		}
		DataflowScene->UnregisterSceneElements();
		DataflowScene->ModifySceneElements().Reset();
		
		TObjectPtr<UDataflowDebugDrawComponent>& DebugDrawComponent = DataflowScene->ModifyDebugDrawComponent();
		const TUniquePtr<IDataflowDebugDrawInterface> DebugDrawObject =
				CreateDebugDrawComponent(DataflowScene, DebugDrawComponent);

		TSharedPtr<FDataflowBaseElement> RootSceneElement = MakeShared<FDataflowBaseElement>(RootName, nullptr, FBox(ForceInitToZero), bIsConstruction);
		DataflowScene->ModifySceneElements().Add(RootSceneElement);

		FName CurrentViewMode;
		if (UDataflowEditorMode* const DataflowMode = Cast<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
		{
			if (const UE::Dataflow::IDataflowConstructionViewMode* const ConstructionViewMode = DataflowMode->GetConstructionViewMode())
			{
				CurrentViewMode = ConstructionViewMode->GetName();
			}
		}
	
		if (const TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			if (const UDataflowEdNode* const SelectedNode = EditorContent->GetSelectedNode())
			{
				if (const TSharedPtr<const FDataflowNode> SelectedDataflowNode = SelectedNode->GetDataflowNode())
				{
					if(SelectedDataflowNode->IsA(NodeType::StaticType()))
					{
						if (SelectedDataflowNode->CanDebugDrawViewMode(CurrentViewMode))
						{
							DebugDrawObject->ResetAllState();

							FDataflowNode::FDebugDrawParameters DebugDrawParameters;
							DebugDrawParameters.bNodeIsSelected = true;
							DebugDrawParameters.bNodeIsPinned = false;
							DebugDrawParameters.CurrentViewMode = CurrentViewMode;
							SelectedDataflowNode->DebugDraw(*DataflowContext, *DebugDrawObject, DebugDrawParameters);
						}
					}
				}
			}

			for (const UDataflowEdNode* const PinnedNode : EditorContent->GetDataflowAsset()->GetWireframeRenderTargets())
			{
				if (const TSharedPtr<const FDataflowNode> PinnedDataflowNode = PinnedNode->GetDataflowNode())
				{
					if(PinnedDataflowNode->IsA(NodeType::StaticType()))
					{
						if (PinnedDataflowNode->CanDebugDrawViewMode(CurrentViewMode))
						{
							DebugDrawObject->ResetAllState();

							FDataflowNode::FDebugDrawParameters DebugDrawParameters;
							DebugDrawParameters.bNodeIsSelected = false;
							DebugDrawParameters.bNodeIsPinned = true;
							DebugDrawParameters.CurrentViewMode = CurrentViewMode;
							PinnedDataflowNode->DebugDraw(*DataflowContext, *DebugDrawObject, DebugDrawParameters);
						}
					}
				}
			}
		}

		DataflowScene->RegisterSceneElements(bIsConstruction);

		DebugDrawComponent->UpdateBounds();
		DebugDrawComponent->MarkRenderTransformDirty();

		DebugDrawOverlay = DebugDrawObject->GetOverlayText();
	}

	static TSharedPtr<FDataflowEditorToolkit> GetAssetEditorToolkitFromMenuContext(UAssetEditorToolkitMenuContext* Context)
	{
		if (Context)
		{
			if (const TSharedPtr<FAssetEditorToolkit> Toolkit = Context->Toolkit.Pin())
			{
				if (Toolkit->GetToolkitFName() == DataflowEditorToolkitName)  // Note: This will not detect subclasses of FDataflowEditorToolkit
				{
					return StaticCastSharedPtr<FDataflowEditorToolkit>(Toolkit);
				}
			}
		}
		return nullptr;
	}
}

FDataflowEditorToolkit::FDataflowEditorToolkit(UAssetEditor* InOwningAssetEditor)
	: FBaseCharacterFXEditorToolkit(InOwningAssetEditor, UE::Dataflow::Private::DataflowEditorToolkitName)
	, DataflowEditor(Cast< UDataflowEditor>(InOwningAssetEditor))
	, bDataflowEnableGraphEval(true)
{
	EvaluationMode = EDataflowEditorEvaluationMode::Automatic;

	// When saving, only prompt to checkout and save assets that are actually modified
	bCheckDirtyOnAssetSave = true;

	check(DataflowEditor);

	ConstructionDefaultLayout = FTabManager::NewLayout(FName("DataflowConstructionLayout03"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.8f)	// Relative width of (Tools Panel, Construction Viewport, Preview Viewport, Dataflow Graph Editor, Outliner) vs (Asset Details, Preview Scene Details, Dataflow Node Details)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
					->SetSizeCoefficient(0.50f)	// Relative height of (Tools Panel, Construction Viewport, Preview Viewport) vs (Dataflow Graph Editor, Outliner)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.1f)		// Relative width of (Tools Panel) vs (Construction Viewport, Preview Viewport)
						->SetExtensionId(UDataflowEditorUISubsystem::EditorSidePanelAreaName)
						->SetHideTabWell(true)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.45f)		// Relative width of (Construction Viewport) vs (Tools Panel, Preview Viewport)
						->AddTab(ViewportTabID, ETabState::OpenedTab)
						->SetExtensionId("ViewportArea")
						->SetHideTabWell(true)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.45f)		// Relative width of (Construction Viewport) vs (Tools Panel, Preview Viewport)
						->AddTab(SimulationViewportTabId, ETabState::OpenedTab)
						->SetExtensionId("ViewportArea")
						->SetHideTabWell(true)
					)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
					->SetSizeCoefficient(0.50f)	// Relative height of (Dataflow Node Details) vs (Asset Details, Preview Scene Details)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.2f)
						->AddTab(MembersWidgetTabId, ETabState::OpenedTab)
						->SetExtensionId("MembersWidgetArea")
						->SetHideTabWell(false)
						->SetForegroundTab(MembersWidgetTabId)
					)
					->Split
					(
						FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
						->SetSizeCoefficient(0.8f)	// Relative width of (Tools Panel, Construction Viewport, Preview Viewport, Dataflow Graph Editor, Outliner) vs (Asset Details, Preview Scene Details, Dataflow Node Details)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(0.8f)	// Relative height of (Dataflow Graph Editor) vs (Dataflow Output log)
							->AddTab(GraphCanvasTabId, ETabState::OpenedTab)
							->AddTab(SubGraphCanvasTabId, ETabState::ClosedTab)
							->SetExtensionId("GraphEditorArea")
							->SetHideTabWell(false)
							->SetForegroundTab(GraphCanvasTabId)
						)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(0.2f)	// Relative height of (Dataflow Graph Editor) vs (Dataflow Output log)
							->AddTab(OutputLogTabId, ETabState::OpenedTab)
							->AddTab(CollectionSpreadSheetTabId_1, ETabState::OpenedTab)
							->SetExtensionId("OutputLogArea")
							->SetHideTabWell(false)
							->SetForegroundTab(OutputLogTabId)
						)
					)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.2f)	// Relative width of (Asset Details, Preview Scene Details, Dataflow Node Details) vs (Tools Panel, Construction Viewport, Preview Viewport, Dataflow Graph Editor, Outliner)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.65f)	// Relative height of (Asset Details, Preview Scene Details) vs (Dataflow Node Details)
					->AddTab(DetailsTabID, ETabState::OpenedTab)
					->AddTab(PreviewSceneTabId, ETabState::OpenedTab)
					->AddTab(SimulationVisualizationTabId, ETabState::OpenedTab)
					->SetExtensionId("DetailsArea")
					->SetHideTabWell(true)
					->SetForegroundTab(DetailsTabID)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)	// Relative height of (Dataflow Node Details) vs (Asset Details, Preview Scene Details)
					->AddTab(NodeDetailsTabId, ETabState::OpenedTab)
					->SetExtensionId("NodeDetailsArea")
					->SetHideTabWell(false)
				)
			)
		);

	SimulationDefaultLayout = FTabManager::NewLayout(FName("DataflowSimulationLayout02"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.8f)	// Relative width of (Tools Panel, Construction Viewport, Preview Viewport, Dataflow Graph Editor, Outliner) vs (Asset Details, Preview Scene Details, Dataflow Node Details)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
					->SetSizeCoefficient(0.60f)	// Relative height of (Tools Panel, Construction Viewport, Preview Viewport) vs (Dataflow Graph Editor, Outliner)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.1f)		// Relative width of (Tools Panel) vs (Construction Viewport, Preview Viewport)
						->SetExtensionId(UDataflowEditorUISubsystem::EditorSidePanelAreaName)
						->SetHideTabWell(true)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.9f)		// Relative width of (Construction Viewport) vs (Tools Panel, Preview Viewport)
						->AddTab(ViewportTabID, ETabState::ClosedTab)
						->AddTab(SimulationViewportTabId, ETabState::OpenedTab)
						->SetExtensionId("ViewportArea")
						->SetHideTabWell(false)
					)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
					->SetSizeCoefficient(0.40f)	// Relative height of (Dataflow Node Details) vs (Asset Details, Preview Scene Details)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.2f)
						->AddTab(CollectionSpreadSheetTabId_1, ETabState::ClosedTab)
						->SetExtensionId("CollectionSpreadSheetArea")
						->SetHideTabWell(false)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.8f)	// Relative height of (Dataflow Graph Editor, Outliner) vs (Tools Panel, Construction Viewport, Preview Viewport)
						->AddTab(GraphCanvasTabId, ETabState::OpenedTab)
						->AddTab(SubGraphCanvasTabId, ETabState::OpenedTab)
						->SetExtensionId("GraphEditorArea")
						->SetHideTabWell(false)
						->SetForegroundTab(GraphCanvasTabId)
					)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.2f)	// Relative width of (Asset Details, Preview Scene Details, Dataflow Node Details) vs (Tools Panel, Construction Viewport, Preview Viewport, Dataflow Graph Editor, Outliner)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.65f)	// Relative height of (Asset Details, Preview Scene Details) vs (Dataflow Node Details)
					->AddTab(DetailsTabID, ETabState::OpenedTab)
					->AddTab(PreviewSceneTabId, ETabState::OpenedTab)
					->SetExtensionId("DetailsArea")
					->SetHideTabWell(true)
					->SetForegroundTab(DetailsTabID)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)	// Relative height of (Dataflow Node Details) vs (Asset Details, Preview Scene Details)
					->AddTab(NodeDetailsTabId, ETabState::OpenedTab)
					->SetExtensionId("NodeDetailsArea")
					->SetHideTabWell(true)
				)
			)
		);

	if(TObjectPtr<UDataflowBaseContent>& EditorContent = DataflowEditor->GetEditorContent())
	{
		if (EditorContent->GetDataflowAsset() && EditorContent->GetDataflowAsset()->Type == EDataflowType::Simulation)
		{
			StandaloneDefaultLayout = SimulationDefaultLayout;
			bForceViewportTab = false;
		}
		else
		{
			StandaloneDefaultLayout = ConstructionDefaultLayout;
			bForceViewportTab = true;
		}
	}

	// Add any extenders specified by the UISubsystem
	// The extenders provide defined locations for FModeToolkit to attach
	// tool palette tabs and detail panel tabs
	LayoutExtender = MakeShared<FLayoutExtender>();
	FDataflowEditorModule* Module = &FModuleManager::LoadModuleChecked<FDataflowEditorModule>("DataflowEditor");
	Module->OnRegisterLayoutExtensions().Broadcast(*LayoutExtender);
	StandaloneDefaultLayout->ProcessExtensions(*LayoutExtender);

	FAdvancedPreviewScene::ConstructionValues PreviewSceneArgs;
	PreviewSceneArgs.bShouldSimulatePhysics = 1;
	PreviewSceneArgs.bCreatePhysicsScene = 1;
	
	TUniquePtr<FDataflowConstructionScene> NewConstructionScene = MakeUnique<FDataflowConstructionScene>(PreviewSceneArgs, DataflowEditor);
	ConstructionScene = NewConstructionScene.Get();
	ObjectScene = MoveTemp(NewConstructionScene);

	SimulationScene = MakeShared<FDataflowSimulationScene>(PreviewSceneArgs, DataflowEditor);

	SimulationSceneProfileIndexStorage = MakeShared<FDataflowSimulationSceneProfileIndexStorage>(SimulationScene.Get());

	IConsoleVariable* const ConsoleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("p.Dataflow.EnableGraphEval"));
	bDataflowEnableGraphEval = ConsoleVar ? ConsoleVar->GetBool() : true;

	if (ConsoleVar)
	{
		GraphEvalCVarChangedDelegateHandle = ConsoleVar->OnChangedDelegate().AddLambda([this](IConsoleVariable* Var)
		{
			bDataflowEnableGraphEval = Var->GetBool();
		});
	}
}

FDataflowEditorToolkit::~FDataflowEditorToolkit()
{
	if (ConstructionScene && ConstructionScene->ModifyDebugDrawComponent())
	{
		ConstructionScene->ModifyDebugDrawComponent()->UnregisterComponent();
	}
	if (SimulationScene && SimulationScene->ModifyDebugDrawComponent())
	{
		SimulationScene->ModifyDebugDrawComponent()->UnregisterComponent();
	}

	if (IConsoleVariable* const ConsoleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("p.Dataflow.EnableGraphEval")))
	{
		ConsoleVar->OnChangedDelegate().Remove(GraphEvalCVarChangedDelegateHandle);
	}

	if (SimulationScene && SimulationScene->GetPreviewSceneDescription())
	{
		SimulationScene->GetPreviewSceneDescription()->DataflowSimulationSceneDescriptionChanged.Remove(OnSimulationSceneChangedDelegateHandle);
	}

	if (GraphEditor)
	{
		GraphEditor->OnSelectionChangedMulticast.Remove(OnSelectionChangedMulticastDelegateHandle);
		GraphEditor->OnNodeDeletedMulticast.Remove(OnNodeDeletedMulticastDelegateHandle);
	}

	UnregisterContextHandlers();

	if (NodeDetailsEditor)
	{
		NodeDetailsEditor->GetOnFinishedChangingPropertiesDelegate().Remove(OnFinishedChangingPropertiesDelegateHandle);
	}

	if (AssetDetailsEditor)
	{
		AssetDetailsEditor->OnFinishedChangingProperties().Remove(OnFinishedChangingAssetPropertiesDelegateHandle);
	}

	if (DataflowOutputLog)
	{
		DataflowOutputLog->GetOnOutputLogMessageTokenClickedDelegate().Remove(OnOutputLogMessageTokenClickedDelegateHandle);
	}

	// We need to force the dataflow editor mode deletion now because otherwise the preview and rest-space worlds
	// will end up getting destroyed before the mode's Exit() function gets to run, and we'll get some
	// warnings when we destroy any mode actors.
	EditorModeManager->DestroyMode(UDataflowEditorMode::EM_DataflowEditorModeId);
	SimulationModeManager->DestroyMode(UDataflowEditorMode::EM_DataflowEditorModeId);
}

void FDataflowEditorToolkit::RegisterContextHandlers()
{
	const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent();
	check(EditorContent);

	if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
	{
		OnNodeBeginEvaluateMulticastDelegateHandle = DataflowContext->OnNodeBeginEvaluateMulticast.AddSP(this, &FDataflowEditorToolkit::OnNodeBeginEvaluate);
		OnNodeFinishEvaluateMulticastDelegateHandle = DataflowContext->OnNodeFinishEvaluateMulticast.AddSP(this, &FDataflowEditorToolkit::OnNodeFinishEvaluate);

		OnContextHasInfoDelegateHandle = DataflowContext->OnContextHasInfo.AddSP(this, &FDataflowEditorToolkit::OnContextHasInfo);
		OnContextHasWarningDelegateHandle = DataflowContext->OnContextHasWarning.AddSP(this, &FDataflowEditorToolkit::OnContextHasWarning);
		OnContextHasErrorDelegateHandle = DataflowContext->OnContextHasError.AddSP(this, &FDataflowEditorToolkit::OnContextHasError);
	}

	FDataflowAssetDelegates::OnNodeInvalidated.AddSP(this, &FDataflowEditorToolkit::OnNodeInvalidated);
}

void FDataflowEditorToolkit::UnregisterContextHandlers()
{
	const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent();
	check(EditorContent);

	if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
	{
		DataflowContext->OnNodeBeginEvaluateMulticast.Remove(OnNodeBeginEvaluateMulticastDelegateHandle);
		DataflowContext->OnNodeFinishEvaluateMulticast.Remove(OnNodeFinishEvaluateMulticastDelegateHandle);

		DataflowContext->OnContextHasInfo.Remove(OnContextHasInfoDelegateHandle);
		DataflowContext->OnContextHasWarning.Remove(OnContextHasWarningDelegateHandle);
		DataflowContext->OnContextHasError.Remove(OnContextHasErrorDelegateHandle);	
	}

	FDataflowAssetDelegates::OnNodeInvalidated.RemoveAll(this);
}

void FDataflowEditorToolkit::CreateEditorModeManager()
{
	auto SetSelectionName = [](const USelection* SelectionObject)
	{
		if(SelectionObject)
		{
			if(UTypedElementSelectionSet* SelectionSet = SelectionObject->GetElementSelectionSet())
			{
				const FName SelectionSetName(FString::Printf(TEXT("DataflowSelectionSet%p"), SelectionSet));
				SelectionSet->SetNameForTedsIntegration(SelectionSetName);
			}
		}
	};
	
	// Setup the construction manager / scene
	FBaseCharacterFXEditorToolkit::CreateEditorModeManager();
	static_cast<FDataflowPreviewSceneBase*>(ObjectScene.Get())->GetDataflowModeManager()
		= StaticCastSharedPtr<FAssetEditorModeManager>(EditorModeManager);

	SetSelectionName(EditorModeManager->GetSelectedActors());
	SetSelectionName(EditorModeManager->GetSelectedComponents());
	SetSelectionName(EditorModeManager->GetSelectedObjects());

	// Setup the simulation manager / scene
	SimulationModeManager = MakeShared<FAssetEditorModeManager>();
	StaticCastSharedPtr<FAssetEditorModeManager>(SimulationModeManager)->SetPreviewScene(
		SimulationScene.Get());
	static_cast<FDataflowPreviewSceneBase*>(SimulationScene.Get())->GetDataflowModeManager()
		= StaticCastSharedPtr<FAssetEditorModeManager>(SimulationModeManager);
	
	SetSelectionName(SimulationModeManager->GetSelectedActors());
	SetSelectionName(SimulationModeManager->GetSelectedComponents());
	SetSelectionName(SimulationModeManager->GetSelectedObjects());
}

void FDataflowEditorToolkit::NotifyPreChange(FEditPropertyChain* PropertyAboutToChange)
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		ensure(EditorContent);
		if (UDataflow* DataflowAsset = EditorContent->GetDataflowAsset())
		{
			FDataflowEditorCommands::OnNotifyPropertyPreChange(NodeDetailsEditor, DataflowAsset, PropertyAboutToChange);
		}
	}
}

bool FDataflowEditorToolkit::CanOpenDataflowEditor(UObject* ObjectToEdit)
{
	return UE::Dataflow::InstanceUtils::HasValidDataflowAsset(ObjectToEdit);
}

bool FDataflowEditorToolkit::HasDataflowAsset(UObject* ObjectToEdit)
{
	return (GetDataflowAsset(ObjectToEdit) != nullptr);
}

UDataflow* FDataflowEditorToolkit::GetDataflowAsset(UObject* ObjectToEdit)
{
	return UE::Dataflow::InstanceUtils::GetDataflowAssetFromObject(ObjectToEdit);
}

const UDataflow* FDataflowEditorToolkit::GetDataflowAsset(const UObject* ObjectToEdit)
{
	return UE::Dataflow::InstanceUtils::GetDataflowAssetFromObject(ObjectToEdit);
}

bool FDataflowEditorToolkit::IsSimulationDataflowAsset() const
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		return (EditorContent->GetDataflowAsset() && EditorContent->GetDataflowAsset()->Type == EDataflowType::Simulation);
	}
	return false;
}

FName FDataflowEditorToolkit::GetGraphLogName() const
{
	static FName CSimulationGraphName = TEXT("Simulation");
	static FName CConstructionGraphName = TEXT("Construction");

	if (IsSimulationDataflowAsset())
	{
		return CSimulationGraphName;
	}

	return CConstructionGraphName;
}

//~ Begin FBaseCharacterFXEditorToolkit overrides

FEditorModeID FDataflowEditorToolkit::GetEditorModeId() const
{
	return UDataflowEditorMode::EM_DataflowEditorModeId;
}

TObjectPtr<UDataflowBaseContent>& FDataflowEditorToolkit::GetEditorContent()
{
	return DataflowEditor->GetEditorContent();
}

const TObjectPtr<UDataflowBaseContent>& FDataflowEditorToolkit::GetEditorContent() const
{
	return DataflowEditor->GetEditorContent();
}

TArray<TObjectPtr<UDataflowBaseContent>>& FDataflowEditorToolkit::GetTerminalContents()
{
	return DataflowEditor->GetTerminalContents();
}

const TArray<TObjectPtr<UDataflowBaseContent>>& FDataflowEditorToolkit::GetTerminalContents() const
{
	return DataflowEditor->GetTerminalContents();
}

bool FDataflowEditorToolkit::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	// Note: This needs a bit of adjusting, because currently OnRequestClose seems to be 
	// called multiple times when the editor itself is being closed. We can take the route 
	// of NiagaraScriptToolkit and remember when changes are discarded, but this can cause
	// issues if the editor close sequence is interrupted due to some other asset editor.

	UDataflowEditorMode* DataflowEdMode = Cast<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId));
	if (!DataflowEdMode) {
		// If we don't have a valid mode, because the OnRequestClose is currently being called multiple times,
		// simply return true because there's nothing left to do.
		return true;
	}

	// Give any active modes a chance to shutdown while the toolkit host is still alive
	// This is super important to do, otherwise currently opened tabs won't be marked as "closed".
	// This results in tabs not being properly recycled upon reopening the editor and tab
	// duplication for each opening event.
	GetEditorModeManager().ActivateDefaultMode();

	return FAssetEditorToolkit::OnRequestClose(InCloseReason);
}

void FDataflowEditorToolkit::PostInitAssetEditor()
{
	FBaseCharacterFXEditorToolkit::PostInitAssetEditor();
	
	auto SetCommonViewportClientOptions = [](FEditorViewportClient* Client)
	{
		// Normally the bIsRealtime flag is determined by whether the connection is remote, but our
		// tools require always being ticked.
		Client->SetRealtime(true);

		// Disable motion blur effects that cause our renders to "fade in" as things are moved
		Client->EngineShowFlags.SetTemporalAA(false);
		Client->EngineShowFlags.SetAntiAliasing(true);
		Client->EngineShowFlags.SetMotionBlur(false);

		// Disable the dithering of occluded portions of gizmos.
		Client->EngineShowFlags.SetOpaqueCompositeEditorPrimitives(true);

		// Disable hardware occlusion queries, which make it harder to use vertex shaders to pull materials
		// toward camera for z ordering because non-translucent materials start occluding themselves (once
		// the component bounds are behind the displaced geometry).
		Client->EngineShowFlags.SetDisableOcclusionQueries(true);

		// Ortho has too many problems with rendering things, unfortunately, so we should use perspective.
		Client->SetViewportType(ELevelViewportType::LVT_Perspective);

		// Lit gives us the most options in terms of the materials we can use.
		Client->SetViewMode(EViewModeIndex::VMI_Lit);

		// We need the viewport client to start out focused, or else it won't get ticked until
		// we click inside it.
		if(Client->Viewport)
		{
			Client->ReceivedFocus(Client->Viewport);
		}
	};
	SetCommonViewportClientOptions(ViewportClient.Get());
	SetCommonViewportClientOptions(SimulationViewportClient.Get());


	UDataflowEditorMode* const DataflowMode = CastChecked<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId));
	const TWeakPtr<FViewportClient> WeakConstructionViewportClient(ViewportClient);
	DataflowMode->SetConstructionViewportClient(StaticCastWeakPtr<FDataflowConstructionViewportClient>(WeakConstructionViewportClient));
	const TWeakPtr<FViewportClient> WeakSimulationViewportClient(SimulationViewportClient);
	DataflowMode->SetSimulationViewportClient(StaticCastWeakPtr<FDataflowSimulationViewportClient>(WeakSimulationViewportClient));

	FDataflowConstructionViewportClient* ConstructionViewportClient = static_cast<FDataflowConstructionViewportClient*>(ViewportClient.Get());
	OnConstructionSelectionChangedDelegateHandle = ConstructionViewportClient->OnSelectionChangedMulticast.AddSP(this, &FDataflowEditorToolkit::OnConstructionViewSelectionChanged);
	OnSimulationSelectionChangedDelegateHandle = SimulationViewportClient->OnSelectionChangedMulticast.AddSP(this, &FDataflowEditorToolkit::OnSimulationViewSelectionChanged);

	// Populate editor toolbar

	FName ParentToolbarName;
	const FName ToolBarName = GetToolMenuToolbarName(ParentToolbarName);
	UToolMenu* const AssetToolbar = UToolMenus::Get()->ExtendMenu(ToolBarName);
	FToolMenuSection& Section = AssetToolbar->FindOrAddSection("DataflowTools");

	AddEvaluationWidget(Section);

	for (const TPair<FName, TSharedPtr<const FUICommandInfo>>& NodeAndAddCommand : DataflowMode->NodeTypeToAddNodeCommandMap)
	{
		ToolkitCommands->MapAction(NodeAndAddCommand.Value,
			FExecuteAction::CreateUObject(DataflowMode, &UDataflowEditorMode::AddNode, NodeAndAddCommand.Key),
			FCanExecuteAction::CreateUObject(DataflowMode, &UDataflowEditorMode::CanAddNode, NodeAndAddCommand.Key));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(NodeAndAddCommand.Value));
	}

	// Force scenes to update loaded asset viewer settings
	UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().Broadcast(NAME_None);
}

void FDataflowEditorToolkit::SetEvaluateGraphMode(EDataflowEditorEvaluationMode Mode)
{
	if (Mode != EvaluationMode)
	{
		EvaluationMode = Mode;
		if (UDataflowEditorOptions* const Options = UDataflowEditorOptions::StaticClass()->GetDefaultObject<UDataflowEditorOptions>())
		{
			Options->EditorEvaluationMode = EvaluationMode;
		}
		// when going back to automatic, make sure the graph is up to date
		if (EvaluationMode == EDataflowEditorEvaluationMode::Automatic)
		{
			EvaluateGraph();
		}
	}
}

void FDataflowEditorToolkit::TogglePerfData()
{
	if (TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			DataflowContext->EnablePerfData(!DataflowContext->IsPerfDataEnabled());
		}
	}
}

bool FDataflowEditorToolkit::IsPerfDataEnabled() const
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			return DataflowContext->IsPerfDataEnabled();
		}
	}
	return false;
}

void FDataflowEditorToolkit::ToggleAsyncEvaluation()
{
	if (TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			if (DataflowContext->IsThreaded())
			{
				DataflowContext->CancelAsyncEvaluation();
			}
			DataflowContext->SetThreaded(!DataflowContext->IsThreaded());
		}
	}
}

bool FDataflowEditorToolkit::IsAsyncEvaluationEnabled() const
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			return DataflowContext->IsThreaded();
		}
	}
	return false;
}

void FDataflowEditorToolkit::ClearGraphCache()
{
	if (TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			DataflowContext->ClearAllData();
		}
	}
}

bool FDataflowEditorToolkit::CanClearGraphCache() const
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			return !DataflowContext->IsEmpty();
		}
	}
	return false;
}

FSlateIcon FDataflowEditorToolkit::GetEvaluationStatusImage() const
{
	static const FName CompileStatusBackground("Blueprint.CompileStatus.Background");
	static const FName CompileStatusUnknown("Blueprint.CompileStatus.Overlay.Unknown");
	static const FName CompileStatusGood("Blueprint.CompileStatus.Overlay.Good");
	static const FName CompileStatusWarning("Blueprint.CompileStatus.Overlay.Warning");
	static const FName CompileStatusError("Blueprint.CompileStatus.Overlay.Error");

	bool bHasWarning = false;
	bool bHasError = false;
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			bHasWarning = (DataflowContext->GetNumWarnings() > 0);
			bHasError = (DataflowContext->GetNumErrors() > 0);
		}
	}

	FName OverlayIcon;
	if (EvaluationMode == EDataflowEditorEvaluationMode::Manual && IsGraphDirty())
	{
		OverlayIcon = CompileStatusUnknown;
	}
	else
	{
		if (bHasError)
		{
			OverlayIcon = CompileStatusError;
		}
		else if (bHasWarning)
		{
			OverlayIcon = CompileStatusWarning;
		}
		else
		{
			OverlayIcon = CompileStatusGood;
		}
	}
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, OverlayIcon);
}

void FDataflowEditorToolkit::AddEvaluationWidget(FToolMenuSection& InSection)
{
	ToolkitCommands->MapAction(FDataflowEditorCommands::Get().EvaluateGraph,
		FExecuteAction::CreateSP(this, &FDataflowEditorToolkit::EvaluateGraph),
		FCanExecuteAction::CreateSP(this, &FDataflowEditorToolkit::IsEvaluateButtonEnabled)
	);
	ToolkitCommands->MapAction(FDataflowEditorCommands::Get().EvaluateGraphAutomatic,
		FExecuteAction::CreateSP(this, &FDataflowEditorToolkit::SetEvaluateGraphMode, EDataflowEditorEvaluationMode::Automatic),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return EvaluationMode == EDataflowEditorEvaluationMode::Automatic; })
	);
	ToolkitCommands->MapAction(FDataflowEditorCommands::Get().EvaluateGraphManual,
		FExecuteAction::CreateSP(this, &FDataflowEditorToolkit::SetEvaluateGraphMode, EDataflowEditorEvaluationMode::Manual),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return EvaluationMode == EDataflowEditorEvaluationMode::Manual; })
	);
	ToolkitCommands->MapAction(FDataflowEditorCommands::Get().ClearGraphCache,  //  TODO: Rename ClearGraphCache to ForceGraphEvaluation in 5.7
		FExecuteAction::CreateLambda([this]() { ClearGraphCache(); EvaluateGraph(); }),  // Note: This action used to only clear the cache, but has been changed to force a re evaluation
		FCanExecuteAction()
	);
	ToolkitCommands->MapAction(FDataflowEditorCommands::Get().TogglePerfData,
		FExecuteAction::CreateSP(this, &FDataflowEditorToolkit::TogglePerfData),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FDataflowEditorToolkit::IsPerfDataEnabled)
	);
	ToolkitCommands->MapAction(FDataflowEditorCommands::Get().ToggleAsyncEvaluation,
		FExecuteAction::CreateSP(this, &FDataflowEditorToolkit::ToggleAsyncEvaluation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FDataflowEditorToolkit::IsAsyncEvaluationEnabled)
	);
	
	InSection.AddDynamicEntry("DataflowEvaluateGraphDynamic", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& DynamicSection)
		{
			using namespace UE::Dataflow::Private;
			if (const TSharedPtr<FDataflowEditorToolkit> Toolkit = GetAssetEditorToolkitFromMenuContext(DynamicSection.FindContext<UAssetEditorToolkitMenuContext>()))
			{
				DynamicSection.AddEntry(FToolMenuEntry::InitToolBarButton(
						FDataflowEditorCommands::Get().EvaluateGraph,
						TAttribute<FText>(),
						TAttribute<FText>(),
						TAttribute<FSlateIcon>(Toolkit.ToSharedRef(), &FDataflowEditorToolkit::GetEvaluationStatusImage)
					));
			}
		}));

	InSection.AddDynamicEntry("DataflowEvaluationOptionsDynamic", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& DynamicSection)
		{
			using namespace UE::Dataflow::Private;
			if (const TSharedPtr<FDataflowEditorToolkit> Toolkit = GetAssetEditorToolkitFromMenuContext(DynamicSection.FindContext<UAssetEditorToolkitMenuContext>()))
			{
				FToolMenuEntry EvaluationOptions = FToolMenuEntry::InitComboButton(
					"DataflowEvaluationOptions",
					FUIAction(),
					FOnGetContent::CreateSP(Toolkit.ToSharedRef(), &FDataflowEditorToolkit::GenerateEvaluationOptionsMenu),
					LOCTEXT("DataflowEvaluationOptions", "Options"),
					LOCTEXT("DataflowEvaluationOptions_ToolbarTooltip", "Options to customize how Dataflow evaluate"),
					TAttribute<FSlateIcon>(),
					true
				);
				EvaluationOptions.StyleNameOverride = "SlimToolBar";
				DynamicSection.AddEntry(EvaluationOptions);
			}
		}));

	// load options
	if (UDataflowEditorOptions* const Options = UDataflowEditorOptions::StaticClass()->GetDefaultObject<UDataflowEditorOptions>())
	{
		EvaluationMode = Options->EditorEvaluationMode;
	}
}

TSharedRef<SWidget> FDataflowEditorToolkit::GenerateEvaluationOptionsMenu()
{
	FMenuBuilder MenuBuilder(true, GetToolkitCommands());
	MenuBuilder.BeginSection(TEXT("Section"));
	MenuBuilder.AddMenuEntry(FDataflowEditorCommands::Get().EvaluateGraphAutomatic);
	MenuBuilder.AddMenuEntry(FDataflowEditorCommands::Get().EvaluateGraphManual);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FDataflowEditorCommands::Get().ClearGraphCache);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FDataflowEditorCommands::Get().TogglePerfData);
	MenuBuilder.AddMenuEntry(FDataflowEditorCommands::Get().ToggleAsyncEvaluation);
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

bool FDataflowEditorToolkit::IsEvaluateButtonEnabled() const
{
	// we disable the button in automatic mode as this should not be needed
	return (EvaluationMode == EDataflowEditorEvaluationMode::Manual);
}

void FDataflowEditorToolkit::InitializeEdMode(UBaseCharacterFXEditorMode* EdMode)
{
	UDataflowEditorMode* DataflowMode = Cast<UDataflowEditorMode>(EdMode);
	check(DataflowMode);
	DataflowMode->SetDataflowEditor(DataflowEditor);

	// We first set the preview scene in order to store the dynamic mesh elements
	// generated by the tools
	DataflowMode->SetDataflowConstructionScene(ConstructionScene);

	checkf(SimulationScene, TEXT("Expected SimulationScene to have been created in FDataflowEditorToolkit constructor"));
	DataflowMode->SetDataflowSimlationScene(SimulationScene.Get());

	// Set of the graph editor to be able to add nodes
	DataflowMode->SetDataflowGraphEditor(GraphEditor);
	TArray<TObjectPtr<UObject>> ObjectsToEdit;
	OwningAssetEditor->GetObjectsToEdit(MutableView(ObjectsToEdit));
	DataflowMode->InitializeTargets(ObjectsToEdit);

	if (TSharedPtr<FModeToolkit> ModeToolkit = DataflowMode->GetToolkit().Pin())
	{
		FDataflowEditorModeToolkit* DataflowModeToolkit = static_cast<FDataflowEditorModeToolkit*>(ModeToolkit.Get());
		DataflowModeToolkit->SetConstructionViewportWidget(DataflowConstructionViewport);
		DataflowModeToolkit->SetSimulationViewportWidget(DataflowSimulationViewport);
	}

	// @todo(brice) : This used to crash when comnmented out. 
	FBaseCharacterFXEditorToolkit::InitializeEdMode(EdMode);
}

void FDataflowEditorToolkit::CreateEditorModeUILayer()
{
	FBaseCharacterFXEditorToolkit::CreateEditorModeUILayer();
}

void FDataflowEditorToolkit::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	FBaseCharacterFXEditorToolkit::GetSaveableObjects(OutObjects);

	if (ensure(GetEditorContent()))
	{
		if (UDataflow* DataflowAsset = GetEditorContent()->GetDataflowAsset())
		{
			check(DataflowAsset->IsAsset());
			OutObjects.AddUnique(DataflowAsset);
		}

		if(SimulationScene && SimulationScene->GetPreviewSceneDescription())
		{
			if(TObjectPtr<UChaosCacheCollection> CacheCollection = SimulationScene->GetPreviewSceneDescription()->CacheAsset)
			{
				OutObjects.AddUnique(CacheCollection);
			}
			if(TObjectPtr<UGeometryCache> GeometryCache = SimulationScene->GetPreviewSceneDescription()->GeometryCacheAsset)
			{
				OutObjects.AddUnique(GeometryCache);
			}
		}
	}
}

//~ End FBaseCharacterFXEditorToolkit overrides

class FDataflowPreviewSceneDescriptionCustomization : public IDetailCustomization
{
public:
	FDataflowPreviewSceneDescriptionCustomization(const TArray<UDataflowBaseContent*>& DataflowContents);

	virtual ~FDataflowPreviewSceneDescriptionCustomization() {}

	
	/**Customize details for the description */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	private :
		/** List of dataflow contents to preview */
		TMap<FString,TArray<UObject*>> ContentTypesObjects;
};

FDataflowPreviewSceneDescriptionCustomization::FDataflowPreviewSceneDescriptionCustomization(const TArray<UDataflowBaseContent*>& DataflowContents) :
	IDetailCustomization(), ContentTypesObjects()
{
	static const FString PreviewCategory = TEXT("Preview");
	TArray<UObject*>& PreviewObjects = ContentTypesObjects.FindOrAdd(PreviewCategory);
	for(UDataflowBaseContent* DataflowContent : DataflowContents)
	{
		if(DataflowContent)
		{
			PreviewObjects.Add(DataflowContent); 
		}
	}
}

void FDataflowPreviewSceneDescriptionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FAddPropertyParams PropertyParams;
	PropertyParams.AllowChildren(true);
	PropertyParams.CreateCategoryNodes(false);
	PropertyParams.HideRootObjectNode(true);
	for(TPair<FString, TArray<UObject*>>& ContentTypeObjects : ContentTypesObjects)
	{
		DetailBuilder.EditCategory(*ContentTypeObjects.Key).AddExternalObjects(ContentTypeObjects.Value, EPropertyLocation::Common, PropertyParams);
	}
}

TSharedRef<class IDetailCustomization> FDataflowEditorToolkit::CustomizePreviewSceneDescription() const
{
	const TArray<UDataflowBaseContent*> SimulationContents  = TArray<UDataflowBaseContent*>{SimulationScene->GetEditorContent()};
	return MakeShareable(new FDataflowPreviewSceneDescriptionCustomization(SimulationContents));
}

TSharedRef<class IDetailCustomization> FDataflowEditorToolkit::CustomizeAssetViewer() const
{
	return MakeShareable(new FDataflowAssetViewerSettingsCustomization(SimulationSceneProfileIndexStorage));
}


//~ Begin FBaseAssetToolkit overrides

void FDataflowEditorToolkit::CreateWidgets()
{
	FBaseCharacterFXEditorToolkit::CreateWidgets();

	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (UDataflow* DataflowAsset = EditorContent->GetDataflowAsset())
		{
			NodeDetailsEditor = CreateNodeDetailsEditorWidget(EditorContent->GetDataflowOwner());
			if(EditorContent->GetDataflowOwner() != EditorContent->GetDataflowAsset())
			{
				AssetDetailsEditor = CreateAssetDetailsEditorWidget({EditorContent->GetDataflowOwner(),EditorContent->GetDataflowAsset()});
			}
			else
			{
				AssetDetailsEditor = CreateAssetDetailsEditorWidget({EditorContent->GetDataflowAsset()});
			}
			GraphEditor = CreateGraphEditorWidget(DataflowAsset, NodeDetailsEditor);

			// Synchronize the EditorContent's selected node with the GraphEditor
			UDataflowEdNode* const InitialSelectedNode = Cast<UDataflowEdNode>(GraphEditor->GetSingleSelectedNode());
			EditorContent->SetSelectedNode(InitialSelectedNode);

			CreateSimulationViewportClient();
		
			TArray<FAdvancedPreviewSceneModule::FDetailCustomizationInfo> DetailsCustomizations;

			DetailsCustomizations.Add({ UDataflowSimulationSceneDescription::StaticClass(),
				FOnGetDetailCustomizationInstance::CreateSP(const_cast<FDataflowEditorToolkit*>(this), &FDataflowEditorToolkit::CustomizePreviewSceneDescription) });

			DetailsCustomizations.Add({ UAssetViewerSettings::StaticClass(),
				FOnGetDetailCustomizationInstance::CreateSP(const_cast<FDataflowEditorToolkit*>(this), &FDataflowEditorToolkit::CustomizeAssetViewer) });

			AdvancedPreviewSettingsWidget = SNew(SDataflowAdvancedPreviewDetailsTab, SimulationScene.ToSharedRef())
				.AdditionalSettings(SimulationScene->GetPreviewSceneDescription())
				.ProfileIndexStorage(SimulationSceneProfileIndexStorage)
				.DetailCustomizations(DetailsCustomizations)
				.PropertyTypeCustomizations(TArray<FAdvancedPreviewSceneModule::FPropertyTypeCustomizationInfo>())
				.Delegates(TArray<FAdvancedPreviewSceneModule::FDetailDelegates>());
			
		}
	}
}

// Called from FBaseAssetToolkit::CreateWidgets. The delegate call path goes through FAssetEditorToolkit::InitAssetEditor
// and FBaseAssetToolkit::SpawnTab_Viewport.
AssetEditorViewportFactoryFunction FDataflowEditorToolkit::GetViewportDelegate()
{
	AssetEditorViewportFactoryFunction TempViewportDelegate = [this](FAssetEditorViewportConstructionArgs InArgs)
	{
		TSharedRef<SDataflowConstructionViewport> Viewport = SAssignNew(DataflowConstructionViewport, SDataflowConstructionViewport, InArgs)
			.ViewportClient(StaticCastSharedPtr<FDataflowConstructionViewportClient>(ViewportClient));
		
		if (UDataflowEditorMode* const DataflowMode = Cast<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
		{
			if (TSharedPtr<FModeToolkit> ModeToolkit = DataflowMode->GetToolkit().Pin())
			{
				if (FDataflowEditorModeToolkit* DataflowModeToolkit = static_cast<FDataflowEditorModeToolkit*>(ModeToolkit.Get()))
				{
					DataflowModeToolkit->SetConstructionViewportWidget(DataflowConstructionViewport);
				}
			}
		}
		return Viewport;
	};

	return TempViewportDelegate;
}

// Called from FBaseAssetToolkit::CreateWidgets to populate ViewportClient, but otherwise only used 
// in our own viewport delegate.
TSharedPtr<FEditorViewportClient> FDataflowEditorToolkit::CreateEditorViewportClient() const
{
	// Note that we can't reliably adjust the viewport client here because we will be passing it
	// into the viewport created by the viewport delegate we get from GetViewportDelegate(), and
	// that delegate may (will) affect the settings based on FAssetEditorViewportConstructionArgs,
	// namely ViewportType.
	// Instead, we do viewport client adjustment in PostInitAssetEditor().
	check(EditorModeManager.IsValid());
	TSharedPtr<FDataflowConstructionViewportClient> LocalConstructionClient = MakeShared<FDataflowConstructionViewportClient>(
	EditorModeManager.Get(), ObjectScene.Get(), true);
	LocalConstructionClient->SetDataflowEditorToolkit(StaticCastSharedRef<FDataflowEditorToolkit>(
		const_cast<FDataflowEditorToolkit*>(this)->AsShared()));

	return LocalConstructionClient;
}

void FDataflowEditorToolkit::CreateSimulationViewportClient()
{
	SimulationTabContent = MakeShareable(new FEditorViewportTabContent());
	SimulationViewportClient = MakeShared<FDataflowSimulationViewportClient>(SimulationModeManager.Get(),
		SimulationScene.Get(), false);
	
	SimulationViewportClient->SetDataflowEditorToolkit(StaticCastSharedRef<FDataflowEditorToolkit>(this->AsShared()));

	SimulationViewportDelegate = [this](FAssetEditorViewportConstructionArgs InArgs)
	{
		TSharedRef<SDataflowSimulationViewport> Viewport = SAssignNew(DataflowSimulationViewport, SDataflowSimulationViewport, InArgs)
			.ViewportClient(StaticCastSharedPtr<FDataflowSimulationViewportClient>(SimulationViewportClient))
			.CommandList(GetToolkitCommands().ToSharedPtr());;

		if (UDataflowEditorMode* const DataflowMode = Cast<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
		{
			if (TSharedPtr<FModeToolkit> ModeToolkit = DataflowMode->GetToolkit().Pin())
			{
				if (FDataflowEditorModeToolkit* DataflowModeToolkit = static_cast<FDataflowEditorModeToolkit*>(ModeToolkit.Get()))
				{
					DataflowModeToolkit->SetSimulationViewportWidget(DataflowSimulationViewport);
				}
			}
		}
		return Viewport;
	};
}

//~ End FBaseAssetToolkit overrides

void FDataflowEditorToolkit::UpdateDebugDraw()
{
	if(TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if(EditorContent->GetDataflowAsset() && EditorContent->GetDataflowAsset()->Type == EDataflowType::Construction)
		{
			static const FString RootElementsName = "Construction Elements";
			UE::Dataflow::Private::UpdateDebugDrawComponent<FDataflowNode>(
				EditorContent, RootElementsName, ConstructionScene, true, DebugDrawOverlayString, EditorModeManager);
		}
		else
		{
			static const FString RootElementsName = "Simulation Elements";
			UE::Dataflow::Private::UpdateDebugDrawComponent<FDataflowSimulationNode>(
				EditorContent, RootElementsName, SimulationScene.Get(), false, DebugDrawOverlayString, SimulationModeManager);
		}
	}
}

namespace UE::Dataflow::Private
{
	static void ShowNotificationMessage(const FText& Message, const SNotificationItem::ECompletionState CompletionState)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 5.0f;
		Info.bUseLargeFont = false;
		Info.bUseThrobber = false;
		Info.bUseSuccessFailIcons = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			Notification->SetCompletionState(CompletionState);
		}
	}
}

void FDataflowEditorToolkit::OnPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		ensure(EditorContent);
		if (UDataflow* DataflowAsset = EditorContent->GetDataflowAsset())
		{
			TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext();
			UE::Dataflow::FTimestamp LastNodeTimestamp = EditorContent->GetLastModifiedTimestamp();
			
			FDataflowEditorCommands::OnPropertyValueChanged(DataflowAsset, DataflowContext, LastNodeTimestamp, PropertyChangedEvent, SelectedDataflowNodes);

			// For manual evaluation no need to invalidate the construction scene if we change any parameters
			// since it will trigger the rendering callbacks with the same cached collection
			const bool bMakesDirty = (EvaluationMode != EDataflowEditorEvaluationMode::Manual);
			EditorContent->SetLastModifiedTimestamp(LastNodeTimestamp, bMakesDirty);

			// Refresh graph display to display nodes with warning/error
			GraphEditor->NotifyGraphChanged();

			UpdateDebugDraw();
		}
	}
}

void FDataflowEditorToolkit::OnAssetPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (IDataflowInstanceInterface* DataflowInstanceInterface = Cast<IDataflowInstanceInterface>(EditorContent->GetDataflowOwner()))
		{
			if (PropertyChangedEvent.GetPropertyName()== FDataflowInstance::GetDataflowAssetPropertyName())
			{
				// close all subgraphs tabs and selection to make sure no other widget refer to this graphs data
				const TArray<TSharedPtr<SDockTab>> Tabs = DocumentManager->GetAllDocumentTabs();
				for (TSharedPtr<SDockTab> Tab : Tabs)
				{
					if (Tab)
					{
						Tab->RequestCloseTab();
					}
				}
				if (GraphEditor)
				{
					GraphEditor->ClearSelectionSet();
				}
				bViewsNeedRefresh = true;
				
				// change the asset and open a new graph in the tab 
				if (UDataflow* NewDataflowAsset = DataflowInstanceInterface->GetDataflowInstance().GetDataflowAsset())
				{
					NewDataflowAsset->Schema = UDataflowSchema::StaticClass();
					EditorContent->SetDataflowAsset(NewDataflowAsset);
					GraphEditor = CreateGraphEditorWidget(NewDataflowAsset, NodeDetailsEditor);
					if (GraphEditorTab)
					{
						if (GraphEditor)
						{
							GraphEditorTab->SetContent(GraphEditor.ToSharedRef());
						}
						else
						{
							GraphEditorTab->SetContent(SNew(SSpacer));
						}
					}
				}
				else
				{
					// Clear the GraphEditor area
					// (Can't have a SDataflowGraphEditor with a null UDataflow, so just put down Spacers if we have no Dataflow)
					GraphEditor.Reset();
					if (GraphEditorTab)
					{
						GraphEditorTab->SetContent(SNew(SSpacer));
					}
				}
			}
		}

		ensure(EditorContent);	FDataflowEditorCommands::OnAssetPropertyValueChanged(EditorContent, PropertyChangedEvent);
	}
}

bool FDataflowEditorToolkit::OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* GraphNode, FText& OutErrorMessage)
{
	return FDataflowEditorCommands::OnNodeVerifyTitleCommit(NewText, GraphNode, OutErrorMessage);
}

void FDataflowEditorToolkit::OnNodeTitleCommitted(const FText& InNewText, ETextCommit::Type InCommitType, UEdGraphNode* GraphNode)
{
	FDataflowEditorCommands::OnNodeTitleCommitted(InNewText, InCommitType, GraphNode);
}

void FDataflowEditorToolkit::OnNodeDoubleClicked(UEdGraphNode* ClickedNode)
{
	// if the node is a call to subgraph , open the subgraph tab 
	if (UDataflowEdNode* DataflowEdNode = Cast<UDataflowEdNode>(ClickedNode))
	{
		if (TSharedPtr<FDataflowNode> DataflowNode = DataflowEdNode->GetDataflowNode())
		{
			if (FDataflowCallSubGraphNode* CallSubGraphNode = DataflowNode->AsType<FDataflowCallSubGraphNode>())
			{
				if (UDataflowSubGraph* Subgraph = GetSubGraph(CallSubGraphNode->GetSubGraphGuid()))
				{
					OpenSubGraphTab(Subgraph);
				}
			}
		}
	}
}

void FDataflowEditorToolkit::OnNodeSelectionChanged(const TSet<UObject*>& InNewSelection)
{
	//
	// Local helper lambdas
	//

	auto FindDataflowNodesInSet = [](const TSet<TObjectPtr<UObject>>& InSet) {
		TSet<TObjectPtr<UObject>> Results;
		for (UObject* Item : InSet)
		{
			if (Cast<UDataflowEdNode>(Item))
			{
				Results.Add(Item);
			}
		}
		return Results;
	};

	auto ResetListeners = [&ViewListeners = ViewListeners](UDataflowEdNode* Node = nullptr)
	{
		for (IDataflowViewListener* Listener : ViewListeners)
		{
			Listener->OnSelectedNodeChanged(nullptr);
		}
		if (Node)
		{
			for (IDataflowViewListener* Listener : ViewListeners)
			{
				Listener->OnSelectedNodeChanged(Node);
			}
		}
	};

	auto IsControlDown = [&DataflowEditor = GraphEditor]()
	{
		if (DataflowEditor)
		{
			return DataflowEditor->IsControlDown();
		}
		return false;
	};

	auto SelectComponentsInView = [&](TObjectPtr<UDataflowEdNode> Node)
	{
		TArray<AActor*> FoundActors;
		if (USelection* SelectedComponents = ConstructionScene->GetDataflowModeManager()->GetSelectedComponents())
		{
			SelectedComponents->Modify();
			SelectedComponents->BeginBatchSelectOperation();

			TArray<TWeakObjectPtr<UObject>> SelectedObjects;
			const int32 NumSelected = SelectedComponents->GetSelectedObjects(SelectedObjects);
			for (TWeakObjectPtr<UObject> WeakObject : SelectedObjects)
			{
				if (WeakObject.IsValid())
				{
					if (UDataflowEditorCollectionComponent* ActorComponent = Cast< UDataflowEditorCollectionComponent>(WeakObject.Get()))
					{
						SelectedComponents->Deselect(ActorComponent);
						ActorComponent->PushSelectionToProxy();
					}
				}
			}

			if (TObjectPtr<AActor> RootActor = ConstructionScene->GetRootActor())
			{
				for (UActorComponent* ActorComponent : RootActor->GetComponents())
				{
					if (UDataflowEditorCollectionComponent* Component = Cast<UDataflowEditorCollectionComponent>(ActorComponent))
					{
						if (Component->Node == Node)
						{
							SelectedComponents->Select(Component);
							Component->PushSelectionToProxy();
						}
					}
				}
			}
			SelectedComponents->EndBatchSelectOperation();
		}
	};

	//
	// Actual function
	// 
	
	// Despite this function's name, we might not have actually changed which node is selected
	bool bPrimarySelectionChanged = false;

	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent(); EditorContent->GetDataflowAsset())
	{
		auto AsObjectPointers = [](const TSet<UObject*>& Set) {
			TSet<TObjectPtr<UObject> > Objs; for (UObject* Elem : Set) Objs.Add(Elem);
			return Objs;
		};

		const TSet<TObjectPtr<UObject>> PreviouslySelectedNodes = SelectedDataflowNodes;
		for (UObject* const PreviouslySelectedNode : SelectedDataflowNodes)
		{
			if (UDataflowEdNode* const PreviouslySelectedEdNode = Cast<UDataflowEdNode>(PreviouslySelectedNode))
			{
				PreviouslySelectedEdNode->SetShouldRenderNode(false);
			}
		}

		// Only keep UDataflowEdNode from NewSelection
		TSet< TObjectPtr<UObject> > NodeSelection = FindDataflowNodesInSet(AsObjectPointers(InNewSelection));

		if (!NodeSelection.Num())
		{
			// The selection is empty. 
			ResetListeners();
			SelectedDataflowNodes = TSet<TObjectPtr<UObject>>();
			if (PrimarySelection) bPrimarySelectionChanged = true;
			PrimarySelection = nullptr;
		}
		else
		{
			TSet<TObjectPtr<UObject>> DeselectedNodes = SelectedDataflowNodes.Difference(NodeSelection);
			TSet<TObjectPtr<UObject>> StillSelectedNodes = SelectedDataflowNodes.Intersect(NodeSelection);
			TSet<TObjectPtr<UObject>> NewlySelectedNodes = NodeSelection.Difference(SelectedDataflowNodes);

			// Something has been removed
			if (DeselectedNodes.Num())
			{
				if (DeselectedNodes.Contains(PrimarySelection))
				{
					ResetListeners();

					if (PrimarySelection) bPrimarySelectionChanged = true;
					PrimarySelection = nullptr;

					// pick a new primary if nothing new was selected
					if (!NewlySelectedNodes.Num() && StillSelectedNodes.Num())
					{
						PrimarySelection = Cast< UDataflowEdNode>(StillSelectedNodes.Array()[0]);
						ResetListeners(PrimarySelection);
						bPrimarySelectionChanged = true;
					}
				}
			}

			// Something new has been selected.
			if (NewlySelectedNodes.Num() >= 1)
			{
				PrimarySelection = Cast< UDataflowEdNode>(NewlySelectedNodes.Array()[0]);
				ResetListeners(PrimarySelection);
				bPrimarySelectionChanged = true;
			}

			SelectedDataflowNodes = NodeSelection;
		}

		for (UObject* const SelectedNode : NodeSelection)
		{
			if (UDataflowEdNode* SelectedEdNode = Cast<UDataflowEdNode>(SelectedNode))
			{
				SelectedEdNode->SetShouldRenderNode(true);
			}
		}

		if (bPrimarySelectionChanged)
		{
			for (UObject* const PreviouslySelectedNode : PreviouslySelectedNodes)
			{
				if (UDataflowEdNode* PreviouslySelectedEdNode = Cast<UDataflowEdNode>(PreviouslySelectedNode))
				{
					PreviouslySelectedEdNode->SetShouldRenderNode(false);
				}
			}

			for (UObject* const SelectedNode : NodeSelection)
			{
				if (UDataflowEdNode* SelectedEdNode = Cast<UDataflowEdNode>(SelectedNode))
				{
					SelectedEdNode->SetShouldRenderNode(true);
				}
			}

			EditorContent->SetSelectedNode(nullptr);

			EditorContent->SetSelectedCollection(nullptr, /*bCollectionIsInput=*/ false);

			if( UDataflowEditorMode* const DataflowMode = Cast<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)) )
			{
				// Close any running tool. OnNodeSingleClicked() will start a new tool if a new node was clicked.

				UEditorInteractiveToolsContext* const ToolsContext = DataflowMode->GetInteractiveToolsContext();
				checkf(ToolsContext, TEXT("No valid ToolsContext found for FDataflowEditorToolkit"));

				UInteractiveToolManager* const ToolManager = ToolsContext->ToolManager;
				checkf(ToolManager, TEXT("ToolsContext doesn't have a valid ToolManager"));

				if (UInteractiveTool* const ActiveTool = ToolManager->GetActiveTool(EToolSide::Left))
				{
					ToolManager->PostActiveToolShutdownRequest(ActiveTool, EToolShutdownType::Accept);
				}

				EditorContent->SetSelectedNode(PrimarySelection);

				// Call the node's OnSelected function. Some nodes use this to cache information from the inputs (e.g. FDataflowCollectionAddScalarVertexPropertyNode::CachedCollectionGroupNames)
				TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext();
				if (PrimarySelection && DataflowContext.IsValid())
				{
					if (const TSharedPtr<FDataflowNode> DataflowNode = PrimarySelection->GetDataflowNode())
					{
						// Update selected Collection in the ContextObject
						for (const FDataflowOutput* const Output : DataflowNode->GetOutputs())
						{
							if (Output->GetType() == FName(TEXT("FManagedArrayCollection")))
							{
								const FManagedArrayCollection DefaultValue;
								TSharedRef<FManagedArrayCollection> Collection = MakeShared<FManagedArrayCollection>(Output->GetValue<FManagedArrayCollection>(*DataflowContext, DefaultValue));
								constexpr bool bCollectionIsInput = false;
								EditorContent->SetSelectedCollection(Collection, bCollectionIsInput);
							}
						}
					}
				}
			}

			if (GetDataflowGraphEditor()->IsAltDown())
			{
				SelectComponentsInView(PrimarySelection);
			}

			UpdateDebugDraw();
		}

		EditorContent->SetConstructionDirty(true);
	}

	//
	// Check if the current view mode can render the selected node. If not, try to find a view mode that can. 
	//

	if (UDataflowEditorMode* const DataflowMode = Cast<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
	{
		if (PrimarySelection && GetEditorContent())
		{
			// Check the most recently used view mode for this "kind" of node -- where "kind" means nodes that have the same set of valid view modes
			auto FindRecentViewMode = [&](FName& OutViewModeName) -> bool
			{
				TArray<FName> ViewModesForNode;
				UE::Dataflow::GetViewModesForNode(*PrimarySelection, *GetEditorContent(), ViewModesForNode);

				if (const FName* const FoundViewMode = DataflowMode->FindRecentlyUsedViewMode(ViewModesForNode))
				{
					OutViewModeName = *FoundViewMode;
					return true;
				}
				return false;
			};

			// Get the first view mode that this node can use
			auto FindAnyValidViewMode = [&](FName& OutViewModeName) -> bool
			{
				for (const TPair<FName, TUniquePtr<UE::Dataflow::IDataflowConstructionViewMode>>& ViewMode : UE::Dataflow::FRenderingViewModeFactory::GetInstance().GetViewModes())
				{
					check(ViewMode.Value.IsValid());
					const bool bCanRender = UE::Dataflow::CanRenderNodeOutput(*PrimarySelection, *GetEditorContent(), *ViewMode.Value);
					if (bCanRender)
					{
						OutViewModeName = ViewMode.Key;
						return true;
					}
				}
			
				return false;
			};

			FName NewViewMode;
			if (FindRecentViewMode(NewViewMode))               // Is there a recently used view mode we can switch to?
			{
				DataflowMode->SetConstructionViewMode(NewViewMode);
			}
			else if (UE::Dataflow::CanRenderNodeOutput(*PrimarySelection, *GetEditorContent(), *DataflowMode->GetConstructionViewMode()))        // Can we use the current view mode?
			{
				// no need to switch
			}		
			else if (FindAnyValidViewMode(NewViewMode))             // Is there *any* view mode that works?
			{
				DataflowMode->SetConstructionViewMode(NewViewMode);
			}
			else                                                    // No valid view mode found
			{
				// TODO: We should clear and disable View Mode Button. For now set default mode to the built-in 3D view mode.
				DataflowMode->SetConstructionViewMode(UE::Dataflow::FDataflowConstruction3DViewMode::Name);
			}
		}

		DataflowMode->SetPendingNodeSelectionChanged(false);
	}
}

bool FDataflowEditorToolkit::IsGraphDirty() const
{
	using namespace UE::Dataflow;

	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			if (UDataflow* DataflowAsset = EditorContent->GetDataflowAsset())
			{
				if (TSharedPtr<UE::Dataflow::FGraph> DataflowGraph = DataflowAsset->GetDataflow())
				{
					const FTimestamp LastModifiedTimestamp = EditorContent->GetLastModifiedTimestamp();

					for (TSharedPtr<FDataflowNode> Node : DataflowGraph->GetNodes())
					{
						if (Node)
						{
							if (FDataflowTerminalNode* TerminalNode = Node->AsType<FDataflowTerminalNode>())
							{
								if (LastModifiedTimestamp < TerminalNode->GetTimestamp())
								{
									return true;
								}
							}
						}
					}
				}
			}
		}
	}
	return false;
}

void FDataflowEditorToolkit::EvaluateGraph()
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (UDataflow* DataflowAsset = EditorContent->GetDataflowAsset())
		{
			if (TSharedPtr<UE::Dataflow::FGraph> DataflowGraph = DataflowAsset->GetDataflow())
			{
				for (const TSharedPtr<FDataflowNode>& DataflowNode : DataflowGraph->GetFilteredNodes(FDataflowTerminalNode::StaticType()))
				{
					if (const FDataflowTerminalNode* TerminalNode = DataflowNode ? DataflowNode->AsType<FDataflowTerminalNode>() : nullptr)
					{
						NodesToEvaluateOnTick.Add(TerminalNode->GetGuid());
					}
				}
			}
		}
	}
}

void FDataflowEditorToolkit::OnNodeInvalidated(UDataflow& DataflowAsset, FDataflowNode& Node)
{
	if (EvaluationMode == EDataflowEditorEvaluationMode::Automatic)
	{
		if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
		{
			if (EditorContent->GetDataflowAsset() == &DataflowAsset)
			{
				// evaluate only active terminals nodes 
				if (FDataflowTerminalNode* TerminalNode = Node.AsType<FDataflowTerminalNode>())
				{
					NodesToEvaluateOnTick.Add(TerminalNode->GetGuid());
				}
			}
		}
	}
}

void FDataflowEditorToolkit::OnNodeSingleClicked(UObject* ClickedNode) const
{
	UDataflowEditorMode* DataflowMode = Cast<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId));
	if (DataflowMode)
	{
		if (GraphEditor && GraphEditor->GetSingleSelectedNode() == ClickedNode)
		{
			// Start the corresponding tool
			DataflowMode->StartToolForSelectedNode(ClickedNode);
		}
	}
}


void FDataflowEditorToolkit::OnNodeDeleted(const TSet<UObject*>& NewSelection)
{
	for (UObject* Node : NewSelection)
	{
		if (SelectedDataflowNodes.Contains(Node))
		{
			SelectedDataflowNodes.Remove(Node);
		}
	}
}

void FDataflowEditorToolkit::OnConstructionViewSelectionChanged(const TArray<UPrimitiveComponent*>& SelectedComponents, const TArray<FDataflowBaseElement*>& SelectedElements)
{
	for(IDataflowViewListener* Listener : ViewListeners)
	{
		Listener->OnConstructionViewSelectionChanged(SelectedComponents, SelectedElements);
	}
}

void FDataflowEditorToolkit::OnSimulationViewSelectionChanged(const TArray<UPrimitiveComponent*>& SelectedComponents, const TArray<FDataflowBaseElement*>& SelectedElements)
{
	for(IDataflowViewListener* Listener : ViewListeners)
	{
		Listener->OnSimulationViewSelectionChanged(SelectedComponents, SelectedElements);
	}
}

void FDataflowEditorToolkit::OnNodeBeginEvaluate(const FDataflowNode* Node, const FDataflowOutput* Output)
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		ensure(EditorContent);
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			DataflowContext->Info(TEXT("Begin Evaluate"), Node, Output);
		}
	}
}

void FDataflowEditorToolkit::OnNodeFinishEvaluate(const FDataflowNode* Node, const FDataflowOutput* Output)
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		ensure(EditorContent);
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			DataflowContext->Info(TEXT("End Evaluate"), Node, Output);
		}
	}
}

void FDataflowEditorToolkit::SetDataflowPathFromNodeAndOutput(const FDataflowNode* Node, const FDataflowOutput* Output, FDataflowPath& OutPath) const
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (const TObjectPtr<UDataflow> DataflowAsset = EditorContent->GetDataflowAsset())
		{
			FName GraphName = GetGraphLogName();
			if (Node)
			{
				if (const UDataflowEdNode* EdNode = DataflowAsset->FindEdNodeByDataflowNodeGuid(Node->GetGuid()))
				{
					if (const UDataflowSubGraph* SubGraph = Cast<UDataflowSubGraph>(EdNode->GetGraph()))
					{
						GraphName = SubGraph->GetFName();
					}
				}
			}
			OutPath.SetGraph(GraphName.ToString());
			OutPath.SetNode(Node ? Node->GetName().ToString() : FString());
			OutPath.SetOutput(Output ? Output->GetName().ToString() : FString());
		}
	}
}

void FDataflowEditorToolkit::LogMessage(const EMessageSeverity::Type Severity, const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Message) const
{
	if (DataflowOutputLog)
	{
		FDataflowPath Path;
		SetDataflowPathFromNodeAndOutput(Node, Output, Path);
		DataflowOutputLog->AddMessage(Severity, Message, Path);
	}
}

void FDataflowEditorToolkit::OnContextHasInfo(const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Info)
{
	LogMessage(EMessageSeverity::Info, Node, Output, Info);
}

void FDataflowEditorToolkit::OnContextHasWarning(const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Warning)
{
	LogMessage(EMessageSeverity::Warning, Node, Output, Warning);
}

void FDataflowEditorToolkit::OnContextHasError(const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Error)
{
	LogMessage(EMessageSeverity::Error, Node, Output, Error);
}

void FDataflowEditorToolkit::OnBeginEvaluate()
{
	GraphEvaluationBegin = FDateTime::Now();

	if (DataflowOutputLog)
	{
		DataflowOutputLog->ClearMessageLog();
	}

	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		ensure(EditorContent);
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			DataflowContext->ClearNodesData();
		}
	}

	// Refresh graph display to display nodes with warning/error
	GraphEditor->NotifyGraphChanged();
}

namespace UE::Dataflow::Private
{
	static int32 GetMilliseconds(const FTimespan& InTimespan)
	{
		const int64 Ticks = InTimespan.GetTicks();
		return (int32)((Ticks / ETimespan::TicksPerMillisecond) % 1000);
	}

	static FString GetElapsedTimeString(const FDateTime& InGraphEvaluationFinished, const FDateTime& InGraphEvaluationBegin)
	{
		const FTimespan ElapsedTimespan = InGraphEvaluationFinished - InGraphEvaluationBegin;
		const FString ElapsedTimeString = FString::Printf(TEXT("%02dm%02ds%02dms"), ElapsedTimespan.GetMinutes(), ElapsedTimespan.GetSeconds(), GetMilliseconds(ElapsedTimespan));

		return ElapsedTimeString;
	}
}

void FDataflowEditorToolkit::OnFinishEvaluate()
{
	// Display message stating that evaluation finished
	const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent();
	ensure(EditorContent);

	if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
	{
		const int32 NumWarnings = DataflowContext->GetNumWarnings();
		const int32 NumErrors = DataflowContext->GetNumErrors();

		if (NumWarnings > 0 || NumErrors > 0)
		{
			const FText MessageFormat = LOCTEXT("OnFinishEvaluate", "Finished graph evaluation\nWarning(s): {0} Error(s): {1}");
			UE::Dataflow::Private::ShowNotificationMessage(FText::Format(MessageFormat, NumWarnings, NumErrors), SNotificationItem::CS_Fail);
		}

		GraphEvaluationFinished = FDateTime::Now();

		const FString ElapsedTimeString = UE::Dataflow::Private::GetElapsedTimeString(GraphEvaluationFinished, GraphEvaluationBegin);
		DataflowContext->Info(TEXT("Evaluation time: ") + ElapsedTimeString);
	}

	// Refresh graph display to update node output pin display (invalid or valid)
	GraphEditor->NotifyGraphChanged();
}

void FDataflowEditorToolkit::OnOutputLogMessageTokenClicked(const FString TokenString)
{
	FDataflowPath Path;
	Path.DecodePath(TokenString);
	const FString NodeName = Path.GetNode();

	FString Output;
	if (!NodeName.IsEmpty() && Path.PathHasOutput())
	{
		Output = Path.GetOutput();
	}

	TSharedPtr<SDataflowGraphEditor> ActiveGraphEditor = GraphEditor;


	// Select node from TokenString
	if (UDataflow* DataflowAsset = GetEditorContent()->GetDataflowAsset())
	{
		const FName GraphName(Path.GetGraph());
		if (UDataflowSubGraph* SubGraph = DataflowAsset->FindSubGraphByName(GraphName))
		{
			OpenSubGraphTab(GraphName);
			ActiveGraphEditor = ActiveSubGraphEditorWeakPtr.Pin();
		}
		else
		{
			// TODO : try to find a way to focus back on the main graph
			//			current issue seem to be related to the tabspawner beeing unknown when calling thos method
			//			maybe a better way would be to leverage the subgraph spawner and show the main graph in the subgraph list 
			//			so that all graphs are spawned and treated the same way ( but this may be an issue to keep the pointer of the main graph )
			//if (TSharedPtr<SDockTab> MainGraphTab = FGlobalTabmanager::Get()->TryInvokeTab(GraphCanvasTabId))
			//{
			//	MainGraphTab->ActivateInParent(SetDirectly);
			//}
		}

		// Clear node selection
		ActiveGraphEditor->ClearSelectionSet();

		// now jump to the relevant node 
		if (TSharedPtr<UE::Dataflow::FGraph> DataflowGraph = DataflowAsset->GetDataflow())
		{
			if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(FName(*NodeName)))
			{
				if (TObjectPtr<UDataflowEdNode> DataflowUEdNode = DataflowAsset->FindEdNodeByDataflowNodeGuid(DataflowNode->GetGuid()))
				{
					ActiveGraphEditor->JumpToNode(DataflowUEdNode);
				}
			}
		}
	}
}

void FDataflowEditorToolkit::Tick(float DeltaTime)
{
	if (const TObjectPtr<UDataflowBaseContent> EditorContent = GetEditorContent())
	{
		if (EditorContent->GetDataflowAsset())
		{
			UE::Dataflow::FTimestamp InitTimeStamp = EditorContent->GetLastModifiedTimestamp();
			if (!EditorContent->GetDataflowContext())
			{
				EditorContent->SetDataflowContext(MakeShared<UE::Dataflow::FEngineContext>(EditorContent->GetDataflowOwner()));
				InitTimeStamp = UE::Dataflow::FTimestamp::Invalid;
			}

			// Update the list of dataflow terminal contents 
			DataflowEditor->UpdateTerminalContents(InitTimeStamp);

			// evaluate the node that are requested from NodesToEvaluateOnTick
			if (NodesToEvaluateOnTick.Num() > 0)
			{
				if (UDataflow* DataflowAsset = EditorContent->GetDataflowAsset())
				{
					if (TSharedPtr<UE::Dataflow::FGraph> DataflowGraph = DataflowAsset->GetDataflow())
					{
						for (const TSharedPtr<FDataflowNode>& DataflowNode : DataflowGraph->GetFilteredNodes(FDataflowTerminalNode::StaticType()))
						{
							if (const FDataflowTerminalNode* TerminalNode = DataflowNode? DataflowNode->AsType<FDataflowTerminalNode>(): nullptr)
							{
								if (NodesToEvaluateOnTick.Contains(TerminalNode->GetGuid()))
								{
									EvaluateTerminalNode(*TerminalNode);
									bViewsNeedRefresh = true;
								}
							}
						}
					}
				}
				NodesToEvaluateOnTick.Reset();
			}
		}
	}
	RefreshViewsIfNeeded();
}

TStatId FDataflowEditorToolkit::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FDataflowEditorToolkit, STATGROUP_Tickables);
}

void FDataflowEditorToolkit::EvaluateTerminalNode(const FDataflowTerminalNode& TerminalNode)
{
	using namespace UE::Dataflow;

	// do not evaluate disabled terminal nodes
	if (!TerminalNode.IsActive())
	{
		return;
	}

	UE_LOG(LogChaosDataflow, VeryVerbose, TEXT("FDataflowEditorToolkit::EvaluateTerminalNode(): Node [%s]"), *TerminalNode.GetName().ToString());

	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		FTimestamp TerminalNodeTimeStamp = FTimestamp::Invalid;

		OnBeginEvaluate();
		EvaluateNode(&TerminalNode, nullptr, TerminalNodeTimeStamp);
		OnFinishEvaluate();

		EditorContent->SetLastModifiedTimestamp(TerminalNodeTimeStamp, /*bMakeDirty*/true);
	}
}

void FDataflowEditorToolkit::EvaluateNode(const FDataflowNode* Node, const FDataflowOutput* Output, UE::Dataflow::FTimestamp& InOutTimestamp)
{
	UE_LOG(LogChaosDataflow, VeryVerbose, TEXT("FDataflowEditorToolkit::EvaluateNode(): Node [%s], Output [%s]"), Node ? *Node->GetName().ToString() : TEXT("nullptr"), Output ? *Output->GetName().ToString() : TEXT("nullptr"));

	if (bDataflowEnableGraphEval)
	{
		const bool bIsInPIEOrSimulate = GEditor->PlayWorld || GEditor->bIsSimulatingInEditor;
		if (!bIsInPIEOrSimulate)  // TODO: make this test optional for some toolkit
		{
			if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
			{
				if (ensure(Node))
				{
					// when evaluation completes, refresh the views
					TWeakPtr<FDataflowEditorToolkit> WeakToolkit = SharedThis(this);
					auto OnEvaluationCompleted = [WeakToolkit](UE::Dataflow::FContext& Context)
						{
							// refresh views 
							if (TSharedPtr<FDataflowEditorToolkit> ToolKit = WeakToolkit.Pin())
							{
								ToolKit->bViewsNeedRefresh = true;
							}
						};
					

					// If Node is null, the terminal node with the given name will be used instead
					FDataflowEditorCommands::EvaluateNode(*EditorContent->GetDataflowContext().Get(), *Node, Output, EditorContent->GetTerminalAsset(), InOutTimestamp, OnEvaluationCompleted);
				}
			}
		}
	}
}

void FDataflowEditorToolkit::RefreshViewsIfNeeded(bool bForce)
{
	if (ConstructionScene->IsSceneDirty() || SimulationScene->IsSceneDirty())
	{
		bViewsNeedRefresh = true;
	}

	if (bForce || bViewsNeedRefresh)
	{
		for (IDataflowViewListener* Listener : ViewListeners)
		{
			Listener->RefreshView();
		}

		bViewsNeedRefresh = false;
	}

	// reset dirty flags
	if (ConstructionScene->IsSceneDirty() || SimulationScene->IsSceneDirty())
	{
		ConstructionScene->ResetDirtyFlag();
		SimulationScene->ResetDirtyFlag();
	}

}

TSharedRef<SDataflowGraphEditor> FDataflowEditorToolkit::CreateGraphEditorWidget(UEdGraph* GraphToEdit, TSharedPtr<IStructureDetailsView> InNodeDetailsEditor)
{
	ensure(GraphToEdit);
	using namespace UE::Dataflow;

	const FDataflowEditorCommands::FGraphEvaluationCallback Evaluate =
		[this](const FDataflowNode* Node, const FDataflowOutput* Output)
		{
			// This method is called when a node is explicily called to be evaluated form the UI 
			// Evaluate may already have happen when the node was invalidated in the graph editor code if the node is part of a branch that 
			// ends with a terminal node and evaluation mode is automatic ( see OnNodeInvalidated )
			// So to avoid double evaluation, we only evaluate if the node is not terminal or if we are in manual evaluation mode
			// todo(dataflow) We should refactor this post 5.6 to make this simpler and less dependent on other method logic
			const bool bIsTerminalNode = (Node && Node->AsType<FDataflowTerminalNode>());
			if (!bIsTerminalNode || EvaluationMode == EDataflowEditorEvaluationMode::Manual)
			{
				if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
				{
					UE::Dataflow::FTimestamp LastNodeTimestamp = EditorContent->GetLastModifiedTimestamp();

					EvaluateNode(Node, Output, LastNodeTimestamp);

					EditorContent->SetLastModifiedTimestamp(LastNodeTimestamp);
				}
			}
		};
	
	DataflowEditor->UpdateTerminalContents(FTimestamp::Invalid);
	
	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnVerifyTextCommit = FOnNodeVerifyTextCommit::CreateSP(this, &FDataflowEditorToolkit::OnNodeVerifyTitleCommit);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FDataflowEditorToolkit::OnNodeTitleCommitted);
	InEvents.OnNodeSingleClicked = SGraphEditor::FOnNodeSingleClicked::CreateSP(this, &FDataflowEditorToolkit::OnNodeSingleClicked);
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FDataflowEditorToolkit::OnNodeDoubleClicked);


	UDataflow* DataflowAsset = UDataflow::GetDataflowAssetFromEdGraph(GraphToEdit);
	check(DataflowAsset);

	TSharedRef<SDataflowGraphEditor> NewGraphEditor = SNew(SDataflowGraphEditor, DataflowAsset)
		.GraphToEdit(GraphToEdit)
		.GraphEvents(InEvents)
		.DetailsView(InNodeDetailsEditor)
		.EvaluateGraph(Evaluate)
		.DataflowEditor(DataflowEditor);

	OnSelectionChangedMulticastDelegateHandle = NewGraphEditor->OnSelectionChangedMulticast.AddSP(this, &FDataflowEditorToolkit::OnNodeSelectionChanged);
	OnNodeDeletedMulticastDelegateHandle = NewGraphEditor->OnNodeDeletedMulticast.AddSP(this, &FDataflowEditorToolkit::OnNodeDeleted);
	
	UnregisterContextHandlers();
	RegisterContextHandlers();

	return NewGraphEditor;
}

TSharedPtr<IStructureDetailsView> FDataflowEditorToolkit::CreateNodeDetailsEditorWidget(UObject* ObjectToEdit)
{
	ensure(ObjectToEdit);
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.NotifyHook = this;
		DetailsViewArgs.bShowOptions = true;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.bShowScrollBar = false;
	}

	FStructureDetailsViewArgs StructureViewArgs;
	{
		StructureViewArgs.bShowObjects = true;
		StructureViewArgs.bShowAssets = true;
		StructureViewArgs.bShowClasses = true;
		StructureViewArgs.bShowInterfaces = true;
	}
	TSharedPtr<IStructureDetailsView> LocalDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr);
	LocalDetailsView->GetDetailsView()->SetObject(ObjectToEdit);
	OnFinishedChangingPropertiesDelegateHandle = LocalDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &FDataflowEditorToolkit::OnPropertyValueChanged);

	NodeDetailsExtensionHandler = MakeShared<UE::Dataflow::FDataflowNodeDetailExtensionHandler>();
	LocalDetailsView->GetDetailsView()->SetExtensionHandler(NodeDetailsExtensionHandler);

	return LocalDetailsView;
}

TSharedPtr<IDetailsView> FDataflowEditorToolkit::CreateAssetDetailsEditorWidget(const TArray<UObject*>& ObjectsToEdit)
{
	ensure(ObjectsToEdit.Num() > 0);
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.NotifyHook = this;
		DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
	}

	TSharedPtr<IDetailsView> LocalDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	LocalDetailsView->SetObjects(ObjectsToEdit, true);

	OnFinishedChangingAssetPropertiesDelegateHandle = LocalDetailsView->OnFinishedChangingProperties().AddSP(this, &FDataflowEditorToolkit::OnAssetPropertyValueChanged);

	return LocalDetailsView;

}

TSharedPtr<SDataflowMembersWidget> FDataflowEditorToolkit::CreateDataflowMembersWidget()
{
	using namespace UE::Dataflow;

	TSharedRef<SDataflowMembersWidget> NewMembersWidget = SNew(SDataflowMembersWidget, SharedThis(this));

	return NewMembersWidget;

}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_AssetDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == DetailsTabID);

	return SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_AssetDetails_TabTitle", "Asset Details"))
		[
			AssetDetailsEditor->AsShared()
		];
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_SimulationViewport(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> DockableTab = SNew(SDockTab);
	if(SimulationTabContent)
	{
		SimulationTabContent->Initialize(SimulationViewportDelegate, DockableTab, SimulationViewportTabId.ToString());
	}
	return DockableTab;
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_PreviewScene(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewSceneTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_PreviewScene_TabTitle", "Preview Scene"))
		[
			AdvancedPreviewSettingsWidget->AsShared()
		];
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_GraphCanvas(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == GraphCanvasTabId);

	SAssignNew(GraphEditorTab, SDockTab)
		.Label(LOCTEXT("DataflowEditor_DataflowGraph_TabTitle", "Dataflow Graph"));

	if (GraphEditor)
	{
		GraphEditorTab.Get()->SetContent(GraphEditor.ToSharedRef());
	}

	return GraphEditorTab.ToSharedRef();
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_SubGraphTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == SubGraphCanvasTabId);

	TSharedPtr<SWidget> SubgraphEditor = SNullWidget::NullWidget;
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (UDataflow* DataflowAsset = EditorContent->GetDataflowAsset())
		{
			SubgraphEditor = CreateGraphEditorWidget(DataflowAsset, NodeDetailsEditor);
		}
	}
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_DataflowSubgraph_TabTitle", "Dataflow Subgraph"))
		[
			SubgraphEditor.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_NodeDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == NodeDetailsTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_NodeDetails_TabTitle", "Node Details"))
		[
			NodeDetailsEditor->GetWidget()->AsShared()
		];
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_SkeletonView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == SkeletonViewTabId);
	check(DataflowEditor);
	check(DataflowEditor->GetEditorContent());

	SkeletonEditorView = MakeShared<FDataflowSkeletonView>(DataflowEditor->GetEditorContent());
	ViewListeners.Add(SkeletonEditorView.Get());

	FSkeletonTreeArgs SkeletonTreeArgs;
	SkeletonTreeArgs.bShowBlendProfiles = false;
	SkeletonTreeArgs.bShowFilterMenu = true;
	SkeletonTreeArgs.bShowDebugVisualizationOptions = false;
	SkeletonTreeArgs.bAllowMeshOperations = false;
	SkeletonTreeArgs.bAllowSkeletonOperations = false;
	SkeletonTreeArgs.bHideBonesByDefault = false;
	SkeletonTreeArgs.OnSelectionChanged = FOnSkeletonTreeSelectionChanged::CreateSP(SkeletonEditorView.ToSharedRef(), &FDataflowSkeletonView::SkeletonViewSelectionChanged);
	SkeletonTreeArgs.ContextName = GetToolkitFName();

	TSharedPtr<ISkeletonTree> SkeletonEditor = SkeletonEditorView->CreateEditor(SkeletonTreeArgs);
	return SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_SkeletonTree_TabTitle", "Skeleton Tree"))
		[
			SkeletonEditor.ToSharedRef()
		];
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_OutlinerView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == OutlinerViewTabId);
	check(DataflowEditor);
	check(DataflowEditor->GetEditorContent());

	DataflowOutlinerView = MakeShared<FDataflowOutlinerView>(ConstructionScene, SimulationScene.Get(), DataflowEditor->GetEditorContent());
	ViewListeners.Add(DataflowOutlinerView.Get());

	TSharedPtr<ISceneOutliner> DataflowOutliner = DataflowOutlinerView->CreateWidget();
	
	return SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_SceneOutliner_TabTitle", "Scene Outliner"))
		[
			DataflowOutliner.ToSharedRef()
		];
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_SelectionView(const FSpawnTabArgs& Args)
{
	//	check(Args.GetTabId().TabType == SelectionViewTabId_1);
	check(DataflowEditor);
	check(DataflowEditor->GetEditorContent());

	if (Args.GetTabId() == SelectionViewTabId_1)
	{
		DataflowSelectionView_1 = MakeShared<FDataflowSelectionView>(DataflowEditor->GetEditorContent());
		if (DataflowSelectionView_1.IsValid())
		{
			ViewListeners.Add(DataflowSelectionView_1.Get());
		}
	}
	else if (Args.GetTabId() == SelectionViewTabId_2)
	{
		DataflowSelectionView_2 = MakeShared<FDataflowSelectionView>(DataflowEditor->GetEditorContent());
		if (DataflowSelectionView_2.IsValid())
		{
			ViewListeners.Add(DataflowSelectionView_2.Get());
		}
	}
	else if (Args.GetTabId() == SelectionViewTabId_3)
	{
		DataflowSelectionView_3 = MakeShared<FDataflowSelectionView>(DataflowEditor->GetEditorContent());
		if (DataflowSelectionView_3.IsValid())
		{
			ViewListeners.Add(DataflowSelectionView_3.Get());
		}
	}
	else if (Args.GetTabId() == SelectionViewTabId_4)
	{
		DataflowSelectionView_4 = MakeShared<FDataflowSelectionView>(DataflowEditor->GetEditorContent());
		if (DataflowSelectionView_4.IsValid())
		{
			ViewListeners.Add(DataflowSelectionView_4.Get());
		}
	}

	TSharedPtr<SSelectionViewWidget> SelectionViewWidget;

	TSharedRef<SDockTab> DockableTab = SNew(SDockTab)
	[
		SAssignNew(SelectionViewWidget, SSelectionViewWidget)
	];

	if (SelectionViewWidget)
	{
		if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
		{
			if (Args.GetTabId() == SelectionViewTabId_1)
			{
				DataflowSelectionView_1->SetSelectionView(SelectionViewWidget);
			}
			else if (Args.GetTabId() == SelectionViewTabId_2)
			{
				DataflowSelectionView_2->SetSelectionView(SelectionViewWidget);
			}
			else if (Args.GetTabId() == SelectionViewTabId_3)
			{
				DataflowSelectionView_3->SetSelectionView(SelectionViewWidget);
			}
			else if (Args.GetTabId() == SelectionViewTabId_4)
			{
				DataflowSelectionView_4->SetSelectionView(SelectionViewWidget);
			}
		}
	}

	DockableTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FDataflowEditorToolkit::OnTabClosed));

	return DockableTab;
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_CollectionSpreadSheet(const FSpawnTabArgs& Args)
{
	check(DataflowEditor);
	check(DataflowEditor->GetEditorContent());

	if (Args.GetTabId() == CollectionSpreadSheetTabId_1)
	{
		DataflowCollectionSpreadSheet_1 = MakeShared<FDataflowCollectionSpreadSheet>(DataflowEditor->GetEditorContent());
		if (DataflowCollectionSpreadSheet_1.IsValid())
		{
			ViewListeners.Add(DataflowCollectionSpreadSheet_1.Get());
		}
	}
	else if (Args.GetTabId() == CollectionSpreadSheetTabId_2)
	{
		DataflowCollectionSpreadSheet_2 = MakeShared<FDataflowCollectionSpreadSheet>(DataflowEditor->GetEditorContent());
		if (DataflowCollectionSpreadSheet_2.IsValid())
		{
			ViewListeners.Add(DataflowCollectionSpreadSheet_2.Get());
		}
	}
	else if (Args.GetTabId() == CollectionSpreadSheetTabId_3)
	{
		DataflowCollectionSpreadSheet_3 = MakeShared<FDataflowCollectionSpreadSheet>(DataflowEditor->GetEditorContent());
		if (DataflowCollectionSpreadSheet_3.IsValid())
		{
			ViewListeners.Add(DataflowCollectionSpreadSheet_3.Get());
		}
	}
	else if (Args.GetTabId() == CollectionSpreadSheetTabId_4)
	{
		DataflowCollectionSpreadSheet_4 = MakeShared<FDataflowCollectionSpreadSheet>(DataflowEditor->GetEditorContent());
		if (DataflowCollectionSpreadSheet_4.IsValid())
		{
			ViewListeners.Add(DataflowCollectionSpreadSheet_4.Get());
		}
	}

	TSharedPtr<SCollectionSpreadSheetWidget> CollectionSpreadSheetWidget;

	TSharedRef<SDockTab> DockableTab = SNew(SDockTab)
	[
		SAssignNew(CollectionSpreadSheetWidget, SCollectionSpreadSheetWidget)
	];

	if (CollectionSpreadSheetWidget)
	{
		if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
		{
			if (Args.GetTabId() == CollectionSpreadSheetTabId_1)
			{
				DataflowCollectionSpreadSheet_1->SetCollectionSpreadSheet(CollectionSpreadSheetWidget);
			}
			else if (Args.GetTabId() == CollectionSpreadSheetTabId_2)
			{
				DataflowCollectionSpreadSheet_2->SetCollectionSpreadSheet(CollectionSpreadSheetWidget);
			}
			else if (Args.GetTabId() == CollectionSpreadSheetTabId_3)
			{
				DataflowCollectionSpreadSheet_3->SetCollectionSpreadSheet(CollectionSpreadSheetWidget);
			}
			else if (Args.GetTabId() == CollectionSpreadSheetTabId_4)
			{
				DataflowCollectionSpreadSheet_4->SetCollectionSpreadSheet(CollectionSpreadSheetWidget);
			}
		}
	}

	DockableTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FDataflowEditorToolkit::OnTabClosed));

	return DockableTab;
}

TSharedPtr<SWidget> FDataflowEditorToolkit::CreateSimulationVisualizationWidget()
{
	FMenuBuilder MenuBuilder(false, nullptr);

	using namespace UE::Dataflow;
	for (const TPair<FName, TUniquePtr<IDataflowSimulationVisualization>>& Visualization : FDataflowSimulationVisualizationRegistry::GetInstance().GetVisualizations())
	{
		Visualization.Value->ExtendSimulationVisualizationMenu(SimulationViewportClient, MenuBuilder);
	}
	return MenuBuilder.MakeWidget();
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_SimulationVisualization(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SimulationVisualizationTab = SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_SimulationVisualisation_TabTitle", "Simulation Visualization"));

	SimulationVisualizationWidget = CreateSimulationVisualizationWidget();
	SimulationVisualizationTab->SetContent(SimulationVisualizationWidget.ToSharedRef());

	// Re-create the visualization panel when the simulation scene changes
	OnSimulationSceneChangedDelegateHandle = SimulationScene->GetPreviewSceneDescription()->DataflowSimulationSceneDescriptionChanged.AddLambda([this, SimulationVisualizationTab]()
	{
		SimulationVisualizationWidget = CreateSimulationVisualizationWidget();
		SimulationVisualizationTab->SetContent(SimulationVisualizationWidget.ToSharedRef());
	});

	return SimulationVisualizationTab;
}


TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_MembersWidget(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> DataflowMembersTab = SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_DataflowMembers_TabTitle", "Dataflow Members"));

	MembersWidget = CreateDataflowMembersWidget();
	DataflowMembersTab->SetContent(MembersWidget.ToSharedRef());

	return DataflowMembersTab;
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_OutputLog(const FSpawnTabArgs& Args)
{
	check(DataflowEditor);
	check(DataflowEditor->GetEditorContent());

	if (Args.GetTabId() == OutputLogTabId)
	{
		DataflowOutputLog = MakeShared<FDataflowOutputLog>(DataflowEditor->GetEditorContent());
	}

	using namespace UE::Dataflow;

	TSharedRef<SDockTab> DockableTab = SNew(SDockTab)
	[
		DataflowOutputLog->GetOutputLogWidget().ToSharedRef()
	];

	if (DataflowOutputLog->GetOutputLogWidget())
	{
		OnOutputLogMessageTokenClickedDelegateHandle = DataflowOutputLog->GetOnOutputLogMessageTokenClickedDelegate().AddRaw(this, &FDataflowEditorToolkit::OnOutputLogMessageTokenClicked);
	}

	DockableTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FDataflowEditorToolkit::OnTabClosed));

	return DockableTab;
}

void FDataflowEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	EditorMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_DataflowEditor", "Dataflow Editor"));
	const TSharedRef<FWorkspaceItem> SelectionViewWorkspaceMenuCategoryRef = EditorMenuCategory->AddGroup(LOCTEXT("WorkspaceMenu_SelectionView", "Selection View"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	const TSharedRef<FWorkspaceItem> CollectionSpreadSheetWorkspaceMenuCategoryRef = EditorMenuCategory->AddGroup(LOCTEXT("WorkspaceMenu_CollectionSpreadSheet", "Collection SpreadSheet"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	
	InTabManager->RegisterTabSpawner(ViewportTabID, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("ConstructionViewportTab", "Construction Viewport"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(SimulationViewportTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SimulationViewport))
		.SetDisplayName(LOCTEXT("SimulationViewportTab", "Simulation Viewport"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(DetailsTabID, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_AssetDetails))
		.SetDisplayName(LOCTEXT("AssetDetailsTab", "Asset Details"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(PreviewSceneTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_PreviewScene))
		.SetDisplayName(LOCTEXT("PreviewSceneTab", "Preview Scene"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
	
	InTabManager->RegisterTabSpawner(GraphCanvasTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_GraphCanvas))
		.SetDisplayName(LOCTEXT("DataflowGraphTab", "Dataflow Graph"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

	InTabManager->RegisterTabSpawner(SubGraphCanvasTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SubGraphTab))
		.SetDisplayName(LOCTEXT("DataflowSubgraphTab", "Dataflow Subgraph"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"))
		.SetMenuType(ETabSpawnerMenuType::Hidden); // hide it from menus as those tabs can only be open by the toolkit itself 

	InTabManager->RegisterTabSpawner(NodeDetailsTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_NodeDetails))
		.SetDisplayName(LOCTEXT("NodeDetailsTab", "Node Details"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	if (bDataflowEnableSkeletonView)
	{
		InTabManager->RegisterTabSpawner(SkeletonViewTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SkeletonView))
			.SetDisplayName(LOCTEXT("SkeletonTreeTab", "Skeleton Tree"))
			.SetGroup(EditorMenuCategory.ToSharedRef())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	}
	
	InTabManager->RegisterTabSpawner(OutlinerViewTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_OutlinerView))
			.SetDisplayName(LOCTEXT("SceneOutlinerTab", "Scene Outliner"))
			.SetGroup(EditorMenuCategory.ToSharedRef())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(SelectionViewTabId_1, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SelectionView))
		.SetDisplayName(LOCTEXT("SelectionViewTab1", "Selection View 1"))
		.SetGroup(SelectionViewWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(SelectionViewTabId_2, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SelectionView))
		.SetDisplayName(LOCTEXT("SelectionViewTab2", "Selection View 2"))
		.SetGroup(SelectionViewWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(SelectionViewTabId_3, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SelectionView))
		.SetDisplayName(LOCTEXT("SelectionViewTab3", "Selection View 3"))
		.SetGroup(SelectionViewWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(SelectionViewTabId_4, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SelectionView))
		.SetDisplayName(LOCTEXT("SelectionViewTab4", "Selection View 4"))
		.SetGroup(SelectionViewWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(CollectionSpreadSheetTabId_1, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_CollectionSpreadSheet))
		.SetDisplayName(LOCTEXT("CollectionSpreadSheetTab1", "Collection SpreadSheet 1"))
		.SetGroup(CollectionSpreadSheetWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(CollectionSpreadSheetTabId_2, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_CollectionSpreadSheet))
		.SetDisplayName(LOCTEXT("CollectionSpreadSheetTab2", "Collection SpreadSheet 2"))
		.SetGroup(CollectionSpreadSheetWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(CollectionSpreadSheetTabId_3, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_CollectionSpreadSheet))
		.SetDisplayName(LOCTEXT("CollectionSpreadSheetTab3", "Collection SpreadSheet 3"))
		.SetGroup(CollectionSpreadSheetWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(CollectionSpreadSheetTabId_4, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_CollectionSpreadSheet))
		.SetDisplayName(LOCTEXT("CollectionSpreadSheetTab4", "Collection SpreadSheet 4"))
		.SetGroup(CollectionSpreadSheetWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(SimulationVisualizationTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SimulationVisualization))
		.SetDisplayName(LOCTEXT("SimulationVisualizationTab", "Simulation Visualization"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	
	InTabManager->RegisterTabSpawner(MembersWidgetTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_MembersWidget))
		.SetDisplayName(LOCTEXT("DataflowMembersTab", "Dataflow Members"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	// Create a document manager to be able to spawn subgraph tabs
	DocumentManager = MakeShareable(new FDocumentTracker(SubGraphCanvasTabId));
	DocumentManager->Initialize(SharedThis(this));
	DocumentManager->SetTabManager(InTabManager);

	TSharedPtr<FDataflowEditorSubGraphTabSummoner> SubGraphTabSummoner = 
		MakeShareable( new FDataflowEditorSubGraphTabSummoner(
			SharedThis(this),
			FDataflowEditorSubGraphTabSummoner::FOnCreateGraphEditorWidget::CreateSP(this, &FDataflowEditorToolkit::CreateSubGraphEditorWidget)
		));
	DocumentManager->RegisterDocumentFactory(SubGraphTabSummoner);
	
	InTabManager->RegisterTabSpawner(OutputLogTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_OutputLog))
		.SetDisplayName(LOCTEXT("OutputLogTab", "Output Log"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Log.TabIcon"));
}

void FDataflowEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FBaseAssetToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(ViewportTabID);
	InTabManager->UnregisterTabSpawner(SimulationViewportTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabID);
	InTabManager->UnregisterTabSpawner(PreviewSceneTabId);
	InTabManager->UnregisterTabSpawner(GraphCanvasTabId);
	InTabManager->UnregisterTabSpawner(SubGraphCanvasTabId);
	InTabManager->UnregisterTabSpawner(NodeDetailsTabId);
	InTabManager->UnregisterTabSpawner(SkeletonViewTabId);
	InTabManager->UnregisterTabSpawner(OutlinerViewTabId);
	InTabManager->UnregisterTabSpawner(SelectionViewTabId_1);
	InTabManager->UnregisterTabSpawner(SelectionViewTabId_2);
	InTabManager->UnregisterTabSpawner(SelectionViewTabId_3);
	InTabManager->UnregisterTabSpawner(SelectionViewTabId_4);
	InTabManager->UnregisterTabSpawner(CollectionSpreadSheetTabId_1);
	InTabManager->UnregisterTabSpawner(CollectionSpreadSheetTabId_2);
	InTabManager->UnregisterTabSpawner(CollectionSpreadSheetTabId_3);
	InTabManager->UnregisterTabSpawner(CollectionSpreadSheetTabId_4);
	InTabManager->UnregisterTabSpawner(SimulationVisualizationTabId);
	InTabManager->UnregisterTabSpawner(MembersWidgetTabId);
	InTabManager->UnregisterTabSpawner(OutputLogTabId);
}

void FDataflowEditorToolkit::OnTabClosed(TSharedRef<SDockTab> Tab)
{
	if (Tab->GetTabLabel().EqualTo(FText::FromString("Selection View 1")))
	{
		ViewListeners.Remove(DataflowSelectionView_1.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Selection View 2")))
	{
		ViewListeners.Remove(DataflowSelectionView_2.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Selection View 3")))
	{
		ViewListeners.Remove(DataflowSelectionView_3.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Selection View 4")))
	{
		ViewListeners.Remove(DataflowSelectionView_4.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Collection SpreadSheet 1")))
	{
		ViewListeners.Remove(DataflowCollectionSpreadSheet_1.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Collection SpreadSheet 2")))
	{
		ViewListeners.Remove(DataflowCollectionSpreadSheet_2.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Collection SpreadSheet 3")))
	{
		ViewListeners.Remove(DataflowCollectionSpreadSheet_3.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Collection SpreadSheet 4")))
	{
		ViewListeners.Remove(DataflowCollectionSpreadSheet_4.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Skeleton Tree")))
	{
		ViewListeners.Remove(SkeletonEditorView.Get());
	}
}

void FDataflowEditorToolkit::SetSubGraphTabActiveState(TSharedPtr<SDataflowGraphEditor> SubGraphEditor, bool bActive)
{
	if (bActive)
	{
		ActiveSubGraphEditorWeakPtr = SubGraphEditor;
	}
	else
	{
		// only reset to null if this was th previously active subgraph
		if (ActiveSubGraphEditorWeakPtr == SubGraphEditor)
		{
			ActiveSubGraphEditorWeakPtr.Reset();
		}
	}
}

UDataflowSubGraph* FDataflowEditorToolkit::GetSubGraph(FName SubGraphName) const
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		ensure(EditorContent);
		if (UDataflow* DataflowAsset = EditorContent->GetDataflowAsset())
		{
			return DataflowAsset->FindSubGraphByName(SubGraphName);
		}
	}
	return nullptr;
}

UDataflowSubGraph* FDataflowEditorToolkit::GetSubGraph(const FGuid& SubGraphGuid) const
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		ensure(EditorContent);
		if (UDataflow* DataflowAsset = EditorContent->GetDataflowAsset())
		{
			return DataflowAsset->FindSubGraphByGuid(SubGraphGuid);
		}
	}
	return nullptr;
}

void FDataflowEditorToolkit::OpenSubGraphTab(FName SubGraphName)
{
	OpenSubGraphTab(GetSubGraph(SubGraphName));
}

void FDataflowEditorToolkit::OpenSubGraphTab(const UDataflowSubGraph* SubGraph)
{
	if (SubGraph)
	{
		TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(SubGraph);
		DocumentManager->OpenDocument(Payload, FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
	}
}

void FDataflowEditorToolkit::CloseSubGraphTab(const UDataflowSubGraph* SubGraph)
{
	if (SubGraph)
	{
		TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(SubGraph);
		DocumentManager->CloseTab(Payload);
	}
}

void FDataflowEditorToolkit::ReOpenSubGraphTab(const UDataflowSubGraph* SubGraph)
{
	CloseSubGraphTab(SubGraph);
	OpenSubGraphTab(SubGraph);
}

TSharedRef<SGraphEditor> FDataflowEditorToolkit::CreateSubGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UDataflowSubGraph* InGraph)
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		return CreateGraphEditorWidget(InGraph, NodeDetailsEditor);
	}
	return SNew(SGraphEditor);
}

FName FDataflowEditorToolkit::GetToolkitFName() const
{
	return FName("DataflowEditor");
}

FText FDataflowEditorToolkit::GetToolkitName() const
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (EditorContent->GetDataflowOwner())
		{
			return  GetLabelForObject(EditorContent->GetDataflowOwner());
		}
		else if (EditorContent->GetDataflowAsset())
		{
			return  GetLabelForObject(EditorContent->GetDataflowAsset());
		}
	}
	return  LOCTEXT("ToolkitName", "Empty Dataflow Editor");
}

FText FDataflowEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Dataflow Editor");
}

FText FDataflowEditorToolkit::GetToolkitToolTipText() const
{
	return  LOCTEXT("ToolkitToolTipText", "Dataflow Editor");
}

FString FDataflowEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Dataflow").ToString();
}

FLinearColor FDataflowEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

FDataflowConstructionScene* FDataflowEditorToolkit::GetConstructionScene() const
{ 
	return ConstructionScene;
}

void FDataflowEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(SelectedDataflowNodes);
	Collector.AddReferencedObject(PrimarySelection);
	if(ConstructionScene)
	{
		Collector.AddReferencedObject(ConstructionScene->ModifyDebugDrawComponent());
	}
	if(SimulationScene)
	{
		Collector.AddReferencedObject(SimulationScene->ModifyDebugDrawComponent());
	}
}

const FString& FDataflowEditorToolkit::GetDebugDrawOverlayString() const
{
	return DebugDrawOverlayString;
}

#undef LOCTEXT_NAMESPACE
