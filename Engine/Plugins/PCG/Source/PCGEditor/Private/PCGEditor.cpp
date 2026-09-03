// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditor.h"

#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGEditorModule.h"
#include "PCGGraph.h"
#include "PCGGraphFactory.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Editor/IPCGEditorModule.h"
#include "Elements/PCGReroute.h"
#include "Helpers/PCGSubgraphHelpers.h"
#include "Rendering/SlateRenderer.h"
#include "Tests/Determinism/PCGDeterminismNativeTests.h"
#include "Tests/Determinism/PCGDeterminismTestBlueprintBase.h"
#include "Tests/Determinism/PCGDeterminismTestsCommon.h"

#include "PCGEditorCommands.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphSchema.h"
#include "PCGEditorGraphSchemaActions.h"
#include "PCGEditorMenuContext.h"
#include "PCGEditorSettings.h"
#include "PCGEditorStyle.h"
#include "PCGEditorUtils.h"
#include "Nodes/PCGEditorGraphNode.h"
#include "Nodes/PCGEditorGraphNodeInput.h"
#include "Nodes/PCGEditorGraphNodeOutput.h"
#include "Nodes/PCGEditorGraphNodeReroute.h"

#include "Widgets/SPCGEditorGraphActionWidget.h"
#include "Widgets/SPCGEditorGraphAttributeListView.h"
#include "Widgets/SPCGEditorGraphDebugObjectTree.h"
#include "Widgets/SPCGEditorGraphDetailsView.h"
#include "Widgets/SPCGEditorGraphDeterminism.h"
#include "Widgets/SPCGEditorGraphFind.h"
#include "Widgets/SPCGEditorGraphLogView.h"
#include "Widgets/SPCGEditorGraphNodePalette.h"
#include "Widgets/SPCGEditorGraphParamsView.h"
#include "Widgets/SPCGEditorGraphProfilingView.h"
#include "Widgets/SPCGEditorNodeSource.h"
#include "Widgets/AssetEditorViewport/SPCGEditorViewport.h"

#include "AssetToolsModule.h"
#include "EdGraphUtilities.h"
#include "EditorAssetLibrary.h"
#include "GraphEditorActions.h"
#include "IAssetTools.h"
#include "LevelEditor.h"
#include "ScopedTransaction.h"
#include "SGraphEditorActionMenu.h"
#include "ShaderCore.h"
#include "SNodePanel.h"
#include "SourceCodeNavigation.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "UnrealEdGlobals.h"
#include "Algo/AnyOf.h"
#include "Editor/UnrealEdEngine.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ITransaction.h"
#include "Misc/MessageDialog.h"
#include "Misc/TransactionObjectEvent.h"
#include "Preferences/UnrealEdOptions.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "PCGGraphEditor"

namespace FPCGEditor_private
{
	const FName GraphEditorID = FName(TEXT("GraphEditor"));
	const FName PropertyDetailsID[] = {
		FName(TEXT("PropertyDetails")),
		FName(TEXT("PropertyDetails2")),
		FName(TEXT("PropertyDetails3")),
		FName(TEXT("PropertyDetails4")) };
	const FName PaletteID = FName(TEXT("Palette"));
	const FName DebugObjectID = FName(TEXT("DebugObject"));
	const FName AttributesID[] = {
		FName(TEXT("Attributes")),
		FName(TEXT("Attributes2")),
		FName(TEXT("Attributes3")),
		FName(TEXT("Attributes4")) };
	const FName FindID = FName(TEXT("Find"));
	const FName DeterminismID = FName(TEXT("Determinism"));
	const FName ProfilingID = FName(TEXT("Profiling"));
	const FName LogID = FName(TEXT("Log"));
	const FName HLSLSourceID = FName(TEXT("HLSLSource"));
	const FName UserParamsID = FName(TEXT("UserParams"));
	const FName ViewportID[] = {
		FName(TEXT("Viewport")),
		FName(TEXT("Viewport2")),
		FName(TEXT("Viewport3")),
		FName(TEXT("Viewport4")) };

	const FText UserParamsTabName = LOCTEXT("UserParamsTab", "Graph Parameters");
}

UPCGEditorGraph* FPCGEditor::GetPCGEditorGraph(UPCGGraph* InGraph)
{
	if (!InGraph)
	{
		return nullptr;
	}

	if (!InGraph->PCGEditorGraph)
	{
		InGraph->PCGEditorGraph = NewObject<UPCGEditorGraph>(InGraph, UPCGEditorGraph::StaticClass(), NAME_None, RF_Transactional | RF_Transient);
		InGraph->PCGEditorGraph->Schema = UPCGEditorGraphSchema::StaticClass();
		InGraph->PCGEditorGraph->InitFromNodeGraph(InGraph);
	}

	return InGraph->PCGEditorGraph;
}

UPCGEditorGraph* FPCGEditor::GetPCGEditorGraph(const UPCGNode* InNode)
{
	UPCGGraph* PCGGraph = InNode ? Cast<UPCGGraph>(InNode->GetOuter()) : nullptr;
	return GetPCGEditorGraph(PCGGraph);
}

UPCGEditorGraph* FPCGEditor::GetPCGEditorGraph(const UPCGSettings* InSettings)
{
	UPCGNode* PCGNode = InSettings ? Cast<UPCGNode>(InSettings->GetOuter()) : nullptr;
	return GetPCGEditorGraph(PCGNode);
}

void FPCGEditor::Initialize(const EToolkitMode::Type InMode, const TSharedPtr<class IToolkitHost>& InToolkitHost, UPCGGraph* InPCGGraph)
{
	PCGGraphBeingEdited = InPCGGraph;

	// Initializes the UPCGEditorGraph if needed
	GetPCGEditorGraph(InPCGGraph);

	PCGGraphBeingEdited->PCGEditorGraph->SetEditor(SharedThis(this));
	PCGEditorGraph = PCGGraphBeingEdited->PCGEditorGraph;

	for (int PropertyDetailsIndex = 0; PropertyDetailsIndex < 4; ++PropertyDetailsIndex)
	{
		TSharedRef<SPCGEditorGraphDetailsView> PropertyDetailsWidget = SNew(SPCGEditorGraphDetailsView);
		PropertyDetailsWidget->SetEditor(SharedThis(this));
		PropertyDetailsWidget->SetObject(PCGGraphBeingEdited);
		PropertyDetailsWidgets.Add(PropertyDetailsWidget);
	}

	GraphEditorWidget = CreateGraphEditorWidget();
	PaletteWidget = CreatePaletteWidget();
	DebugObjectTreeWidget = CreateDebugObjectTreeWidget();
	FindWidget = CreateFindWidget();

	for (int AttributesIndex = 0; AttributesIndex < 4; ++AttributesIndex)
	{
		const int ViewportEditorPanel = static_cast<int>(EPCGEditorPanel::Viewport1) + AttributesIndex;

		AttributesWidgets.Add(CreateAttributesWidget());
		AttributesWidgets[AttributesIndex]->SetViewportWidget(CreateViewportWidget(), static_cast<EPCGEditorPanel>(ViewportEditorPanel));
	}
	
	DeterminismWidget = CreateDeterminismWidget();
	ProfilingWidget = CreateProfilingWidget();
	LogWidget = CreateLogWidget();
	NodeSourceWidget = CreateNodeSourceWidget();
	UserParamsWidget = CreateGraphParamsWidget();

	BindCommands();
	RegisterToolbar();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = GetDefaultLayout();

	const FName PCGGraphEditorAppName = FName(TEXT("PCGEditorApp"));

	InitAssetEditor(InMode, InToolkitHost, PCGGraphEditorAppName, StandaloneDefaultLayout, /*bCreateDefaultStandaloneMenu=*/ true, /*bCreateDefaultToolbar=*/ true, InPCGGraph);

	PCGGraphBeingEdited->OnGraphChangedDelegate.AddRaw(this, &FPCGEditor::OnGraphChanged);
	PCGGraphBeingEdited->OnNodeSourceCompiledDelegate.AddRaw(this, &FPCGEditor::OnNodeSourceCompiled);

	// Hook to map change / delete actor to refresh debug object selection list, to help prevent it going stale.
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnMapChanged().AddRaw(this, &FPCGEditor::OnMapChanged);
	if (GEngine)
	{
		GEngine->OnLevelActorDeleted().AddRaw(this, &FPCGEditor::OnLevelActorDeleted);
	}

	// Hook to PIE start/end to keep callbacks up to date.
	FEditorDelegates::PostPIEStarted.AddRaw(this, &FPCGEditor::OnPostPIEStarted);
	FEditorDelegates::EndPIE.AddRaw(this, &FPCGEditor::OnEndPIE);

	if (GEditor)
	{
		RegisterDelegatesForWorld(GEditor->GetEditorWorldContext().World());

		// In case the editor is opened while in PIE, we should try setting up callbacks for the PIE world.
		RegisterDelegatesForWorld(GEditor->PlayWorld.Get());
	}

	// Clear inspection flag on all nodes.
	for (UEdGraphNode* EdGraphNode : PCGEditorGraph->Nodes)
	{
		if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(EdGraphNode))
		{
			PCGEditorGraphNode->SetInspected(false);
		}
	}
}

UPCGEditorGraph* FPCGEditor::GetPCGEditorGraph()
{
	return PCGEditorGraph;
}

void FPCGEditor::SetStackBeingInspectedFromAnotherEditor(const FPCGStack& FullStack)
{
	if (DebugObjectTreeWidget)
	{
		DebugObjectTreeWidget->SetDebugObjectFromStackFromAnotherEditor(FullStack);
	}
}

void FPCGEditor::SetStackBeingInspected(const FPCGStack& FullStack)
{
	if (FullStack == StackBeingInspected)
	{
		// No-op if we're already inspecting this stack.
		return;
	}

	UPCGComponent* LastComponent = LastValidPCGComponentBeingInspected.Get();
	UPCGComponent* NewComponent = const_cast<UPCGComponent*>(FullStack.GetRootComponent());

	if (NewComponent && NewComponent != LastComponent)
	{
		if (LastComponent && LastComponent->GetExecutionState().GetInspection().IsInspecting())
		{
			LastComponent->GetExecutionState().GetInspection().DisableInspection();
		}

		LastValidPCGComponentBeingInspected = NewComponent;
	}

	if (PCGGraphBeingEdited)
	{
		if (PCGGraphBeingEdited->IsInspecting())
		{
			PCGGraphBeingEdited->DisableInspection();
		}

		PCGGraphBeingEdited->EnableInspection(StackBeingInspected);
	}

	PCGComponentBeingInspected = NewComponent;

	StackBeingInspected = FullStack;
	OnInspectedStackChangedDelegate.Broadcast(StackBeingInspected);

	UpdateAfterInspectedStackChanged();
}

void FPCGEditor::OnComponentGenerated(UPCGComponent* InComponent)
{
	if (DebugObjectTreeWidget)
	{
		DebugObjectTreeWidget->RequestRefresh();
	}

	if(InComponent == GetPCGComponentBeingInspected())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGEditor::SetStackBeingInspected::BroadcastStackBeingInspected);
		OnInspectedStackChangedDelegate.Broadcast(StackBeingInspected);
	}
}

bool FPCGEditor::OnValidateNodeTitle(const FText& NewName, UEdGraphNode* GraphNode, FText& OutErrorMessage)
{
	if (UPCGEditorGraphNode* PCGGraphNode = Cast<UPCGEditorGraphNode>(GraphNode))
	{
		return PCGGraphNode->OnValidateNodeTitle(NewName, OutErrorMessage);
	}
	else if (GraphNode && GraphNode->IsA<UEdGraphNode_Comment>())
	{
		return true;
	}

	return false;
}

void FPCGEditor::UpdateAfterInspectedStackChanged()
{
	UPCGComponent* Component = PCGComponentBeingInspected.Get();

	if (Component)
	{
		// Implementation note: if we're inspecting and have not pre-run the graph, then it probably makes sense to enable inspection by default. 
		// TODO This could be selected with a cvar though.
		const bool bHasBeenGeneratedThisSession = Component->bGenerated && Component->WasGeneratedThisSession();
		const bool bWasInspecting = Component->GetExecutionState().GetInspection().IsInspecting();
		const bool bNeedsInspection = Algo::AnyOf(AttributesWidgets, [](const TSharedPtr<SPCGEditorGraphAttributeListView>& ALV) { return ALV->GetNodeBeingInspected() != nullptr; });

		if (!bHasBeenGeneratedThisSession || (bNeedsInspection && !bWasInspecting))
		{
			Component->GetExecutionState().GetInspection().EnableInspection();
			UpdateDebugAfterComponentSelection(Component, Component, true);
		}
	}

	check(PCGEditorGraph);
	for (UEdGraphNode* Node : PCGEditorGraph->Nodes)
	{
		if (UPCGEditorGraphNodeBase* PCGNode = Cast<UPCGEditorGraphNodeBase>(Node))
		{
			// Update now that component has changed. Will fire OnNodeChanged if necessary.
			EPCGChangeType ChangeType = PCGNode->UpdateErrorsAndWarnings();
			ChangeType |= PCGNode->UpdateStructuralVisualization(Component, &StackBeingInspected);
			ChangeType |= PCGNode->UpdateGPUVisualization(Component, &StackBeingInspected);

			if (ChangeType != EPCGChangeType::None)
			{
				PCGNode->ReconstructNode();
			}
		}
	}
}

void FPCGEditor::ClearStackBeingInspected()
{
	if (GetStackBeingInspected())
	{
		SetStackBeingInspected(FPCGStack());
	}
}

void FPCGEditor::UpdateDebugAfterComponentSelection(UPCGComponent* InOldComponent, UPCGComponent* InNewComponent, bool bInNewComponentStartedInspecting)
{
	if (!ensure(PCGGraphBeingEdited))
	{
		return;
	}

	auto RefreshComponent = [](UPCGComponent* Component)
	{
		if (!ensure(Component))
		{
			return;
		}

		// GenerateAtRuntime components should be refreshed through the runtime gen scheduler.
		if (Component->IsManagedByRuntimeGenSystem())
		{
			if (UPCGSubsystem* Subsystem = GetSubsystem())
			{
				// We don't want to do a full cleanup if we're setting the debug object, since full cleanup destroys the component, which is the debug object itself!
				Subsystem->RefreshRuntimeGenComponent(Component);
			}
		}
		else
		{
			Component->GenerateLocal(/*bForce=*/true);
		}
	};

	// If individual component debugging is disabled, just generate the new component if required.
	if (!PCGGraphBeingEdited->DebugFlagAppliesToIndividualComponents())
	{
		if (InNewComponent && bInNewComponentStartedInspecting)
		{
			RefreshComponent(InNewComponent);
		}

		return;
	}

	// Trigger necessary generation(s) for per-component debugging.
	if (!InOldComponent)
	{
		if (InNewComponent && bInNewComponentStartedInspecting)
		{
			// Transition from 'null' to 'any component not already inspecting' - generate to create debug/inspection info.
			// If we have null selected, all components are displaying debug. Go to Original component so that all refresh.
			RefreshComponent(InNewComponent->GetOriginalComponent());
		}
	}
	else
	{
		const bool bDebugFlagSetOnAnyNode = Algo::AnyOf(PCGGraphBeingEdited->GetNodes(), [](const UPCGNode* InNode)
		{
			return InNode && InNode->GetSettings() && InNode->GetSettings()->bDebug;
		});

		// Regenerate to clear debug info if switching components, or if changing from a component to null.
		if (InNewComponent != InOldComponent && (InNewComponent || bDebugFlagSetOnAnyNode))
		{
			// Use original component - debug can be displayed both by the local component and parent local components.
			RefreshComponent(InOldComponent->GetOriginalComponent());
		}

		// Debug new component if it wasn't already
		if (InNewComponent && bInNewComponentStartedInspecting)
		{
			// Use original component - debug can be displayed both by the local component and parent local components.
			RefreshComponent(InNewComponent->GetOriginalComponent());
		}
	}
}

const FPCGStack* FPCGEditor::GetStackBeingInspected() const
{
	return StackBeingInspected.GetStackFrames().IsEmpty() ? nullptr : &StackBeingInspected;
}

void FPCGEditor::SetSourceEditorTargetObject(UObject* InObject)
{
	NodeSourceWidget->SetTextProviderObject(InObject);
}

void FPCGEditor::JumpToNode(const UEdGraphNode* InNode)
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->JumpToNode(InNode);
	}
}

UPCGEditorGraphNodeBase* FPCGEditor::GetEditorNode(const UPCGNode* InNode)
{
	if (!ensure(PCGEditorGraph) || !InNode)
	{
		return nullptr;
	}

	for (UEdGraphNode* EdGraphNode : PCGEditorGraph->Nodes)
	{
		if (UPCGEditorGraphNodeBase* PCGEdGraphNode = Cast<UPCGEditorGraphNodeBase>(EdGraphNode))
		{
			if (PCGEdGraphNode->GetPCGNode() == InNode)
			{
				return PCGEdGraphNode;
			}
		}
	}

	return nullptr;
}

void FPCGEditor::JumpToNode(const UPCGNode* InNode)
{
	if (const UPCGEditorGraphNodeBase* EditorNode = GetEditorNode(InNode))
	{
		JumpToNode(EditorNode);
	}
}

FName FPCGEditor::GetPanelID(const EPCGEditorPanel Panel) const
{
	switch (Panel)
	{
		case EPCGEditorPanel::Attributes1:
			return FPCGEditor_private::AttributesID[0];
		case EPCGEditorPanel::Attributes2:
			return FPCGEditor_private::AttributesID[1];
		case EPCGEditorPanel::Attributes3:
			return FPCGEditor_private::AttributesID[2];
		case EPCGEditorPanel::Attributes4:
			return FPCGEditor_private::AttributesID[3];
		case EPCGEditorPanel::DebugObjectTree:
			return FPCGEditor_private::DebugObjectID;
		case EPCGEditorPanel::Determinism:
			return FPCGEditor_private::DeterminismID;
		case EPCGEditorPanel::Find:
			return FPCGEditor_private::FindID;
		case EPCGEditorPanel::GraphEditor:
			return FPCGEditor_private::GraphEditorID;
		case EPCGEditorPanel::Log:
			return FPCGEditor_private::LogID;
		case EPCGEditorPanel::NodePalette:
			return FPCGEditor_private::PaletteID;
		case EPCGEditorPanel::NodeSource:
			return FPCGEditor_private::HLSLSourceID;
		case EPCGEditorPanel::Profiling:
			return FPCGEditor_private::ProfilingID;
		case EPCGEditorPanel::PropertyDetails1:
			return FPCGEditor_private::PropertyDetailsID[0];
		case EPCGEditorPanel::PropertyDetails2:
			return FPCGEditor_private::PropertyDetailsID[1];
		case EPCGEditorPanel::PropertyDetails3:
			return FPCGEditor_private::PropertyDetailsID[2];
		case EPCGEditorPanel::PropertyDetails4:
			return FPCGEditor_private::PropertyDetailsID[3];
		case EPCGEditorPanel::UserParams:
			return FPCGEditor_private::UserParamsID;
		case EPCGEditorPanel::Viewport1:
			return FPCGEditor_private::ViewportID[0];
		case EPCGEditorPanel::Viewport2:
			return FPCGEditor_private::ViewportID[1];
		case EPCGEditorPanel::Viewport3:
			return FPCGEditor_private::ViewportID[2];
		case EPCGEditorPanel::Viewport4:
			return FPCGEditor_private::ViewportID[3];
		default:
			return NAME_None;
	}
}

void FPCGEditor::BringFocusToPanel(const EPCGEditorPanel Panel) const
{
	const FName PanelID = GetPanelID(Panel);
	if (PanelID != NAME_None)
	{
		const TSharedPtr<SDockTab> Tab = TabManager->TryInvokeTab(PanelID);
		Tab->DrawAttention(); // Bring the panel to focus and flash the tab
	}
}

void FPCGEditor::CloseGraphPanel(const EPCGEditorPanel Panel) const
{
	const FName PanelID = GetPanelID(Panel);
	if (PanelID != NAME_None)
	{
		const TSharedPtr<SDockTab> Tab = TabManager->FindExistingLiveTab(PanelID);
		Tab->RequestCloseTab();
	}
}

bool FPCGEditor::IsPanelCurrentlyOpen(const EPCGEditorPanel Panel) const
{
	return TabManager.IsValid() && TabManager->FindExistingLiveTab(GetPanelID(Panel));
}

bool FPCGEditor::IsPanelCurrentlyForeground(const EPCGEditorPanel Panel) const
{
		const TSharedPtr<SDockTab> DockTab = TabManager.IsValid() ? TabManager->FindExistingLiveTab(GetPanelID(Panel)) : nullptr;
		return DockTab.IsValid() && DockTab->IsForeground();
}

void FPCGEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_PCGEditor", "PCG Editor"));
	TSharedRef<FWorkspaceItem> DetailsGroup = WorkspaceMenuCategory->AddGroup(LOCTEXT("WorkspaceMenu_PCGEditor_Details", "Details"));
	TSharedRef<FWorkspaceItem> AttributesGroup = WorkspaceMenuCategory->AddGroup(LOCTEXT("WorkspaceMenu_PCGEditor_Attributes", "Attributes"));
	TSharedRef<FWorkspaceItem> ViewportGroup = WorkspaceMenuCategory->AddGroup(LOCTEXT("WorkspaceMenu_PCGEditor_Viewport", "Data Viewport"));
	const TSharedRef<FWorkspaceItem>& WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	//TODO: Add Icons
	InTabManager->RegisterTabSpawner(FPCGEditor_private::GraphEditorID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_GraphEditor))
		.SetDisplayName(LOCTEXT("GraphTab", "Graph"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PropertyDetailsID[0], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_PropertyDetails, 0))
		.SetDisplayName(LOCTEXT("DetailsTab1", "Details 1"))
		.SetGroup(DetailsGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PropertyDetailsID[1], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_PropertyDetails, 1))
		.SetDisplayName(LOCTEXT("DetailsTab2", "Details 2"))
		.SetGroup(DetailsGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PropertyDetailsID[2], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_PropertyDetails, 2))
		.SetDisplayName(LOCTEXT("DetailsTab3", "Details 3"))
		.SetGroup(DetailsGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PropertyDetailsID[3], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_PropertyDetails, 3))
		.SetDisplayName(LOCTEXT("DetailsTab4", "Details 4"))
		.SetGroup(DetailsGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PaletteID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Palette))
		.SetDisplayName(LOCTEXT("PaletteTab", "Palette"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::DebugObjectID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_DebugObjectTree))
		.SetDisplayName(LOCTEXT("DebugTab", "Debug Object Tree"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::AttributesID[0], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Attributes, 0))
		.SetDisplayName(LOCTEXT("AttributesTab1", "Attributes 1"))
		.SetGroup(AttributesGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::AttributesID[1], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Attributes, 1))
		.SetDisplayName(LOCTEXT("AttributesTab2", "Attributes 2"))
		.SetGroup(AttributesGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::AttributesID[2], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Attributes, 2))
		.SetDisplayName(LOCTEXT("AttributesTab3", "Attributes 3"))
		.SetGroup(AttributesGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::AttributesID[3], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Attributes, 3))
		.SetDisplayName(LOCTEXT("AttributesTab4", "Attributes 4"))
		.SetGroup(AttributesGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::FindID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Find))
		.SetDisplayName(LOCTEXT("FindTab", "Find"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::DeterminismID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Determinism))
		.SetDisplayName(LOCTEXT("DeterminismTab", "Determinism"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::ProfilingID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Profiling))
		.SetDisplayName(LOCTEXT("ProfilingTab", "Profiling"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::LogID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Log))
		.SetDisplayName(LOCTEXT("LogCaptureTab", "Log Capture"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::HLSLSourceID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_NodeSource))
		.SetDisplayName(LOCTEXT("NodeSourceTab", "HLSL Source"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::UserParamsID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_UserParams))
		.SetDisplayName(FPCGEditor_private::UserParamsTabName)
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::ViewportID[0], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Viewport, 0))
		.SetDisplayName(LOCTEXT("ViewportTab1", "Data Viewport 1"))
		.SetGroup(ViewportGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::ViewportID[1], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Viewport, 1))
		.SetDisplayName(LOCTEXT("ViewportTab2", "Data Viewport 2"))
		.SetGroup(ViewportGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::ViewportID[2], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Viewport, 2))
		.SetDisplayName(LOCTEXT("ViewportTab3", "Data Viewport 3"))
		.SetGroup(ViewportGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::ViewportID[3], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Viewport, 3))
		.SetDisplayName(LOCTEXT("ViewportTab4", "Data Viewport 4"))
		.SetGroup(ViewportGroup);
}

void FPCGEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::GraphEditorID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PropertyDetailsID[0]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PropertyDetailsID[1]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PropertyDetailsID[2]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PropertyDetailsID[3]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PaletteID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::DebugObjectID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::AttributesID[0]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::AttributesID[1]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::AttributesID[2]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::AttributesID[3]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::FindID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::DeterminismID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::ProfilingID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::LogID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::HLSLSourceID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::UserParamsID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::ViewportID[0]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::ViewportID[1]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::ViewportID[2]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::ViewportID[3]);

	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FPCGEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PCGGraphBeingEdited);

	for (TSharedPtr<SPCGEditorGraphAttributeListView> ALV : AttributesWidgets)
	{
		if (ALV)
		{
			ALV->AddReferencedObjects(Collector);
		}
	}
}

bool FPCGEditor::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
{
	if (InContext.Context == FPCGEditorCommon::ContextIdentifier)
	{
		return true;
	}

	// This is done to catch transaction blocks made outside PCG editor code were we need to trigger PostUndo for our context, i.e. UPCGEditorGraphSchema::TryCreateConnection
	for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectContext : TransactionObjectContexts)
	{
		const UObject* Object = TransactionObjectContext.Key;
		while (Object != nullptr)
		{
			if (Object == PCGGraphBeingEdited)
			{
				return true;
			}
			Object = Object->GetOuter();
		}
	}

	return false;
}

void FPCGEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		if (PCGGraphBeingEdited)
		{
			// Deepest change type to catch all types of change (like redoing adding a grid size node or etc).
			PCGGraphBeingEdited->NotifyGraphChanged(EPCGChangeType::Structural | EPCGChangeType::GenerationGrid);
		}

		if (GraphEditorWidget.IsValid())
		{
			GraphEditorWidget->ClearSelectionSet();
			GraphEditorWidget->NotifyGraphChanged();

			FSlateApplication::Get().DismissAllMenus();
		}
	}
}

FName FPCGEditor::GetToolkitFName() const
{
	return FName(TEXT("PCGEditor"));
}

FText FPCGEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "PCG Editor");
}

FLinearColor FPCGEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

FString FPCGEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "PCG ").ToString();
}

void FPCGEditor::RegisterToolbar() const
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	FName ParentName;
	const FName ToolbarName = GetToolMenuToolbarName(ParentName);
	if (!ToolMenus->IsMenuRegistered(ToolbarName))
	{
		UToolMenu* ToolBar = ToolMenus->RegisterMenu(ToolbarName, ParentName, EMultiBoxType::ToolBar);

		const FPCGEditorCommands& PCGEditorCommands = FPCGEditorCommands::Get();
		FToolMenuSection& Section = ToolBar->AddSection("PCGToolbar", TAttribute<FText>());

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			PCGEditorCommands.Find,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.Find")));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().PauseAutoRegeneration,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.PauseRegen")));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().ForceGraphRegeneration,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>::CreateLambda([]()
			{
				static const FSlateIcon ForceRegen = FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.ForceRegen");
				static const FSlateIcon ForceRegenClearCache = FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.ForceRegenClearCache");
					
				FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
				return ModifierKeys.IsControlDown() ? ForceRegenClearCache : ForceRegen;
			})));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().CancelExecution,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.StopRegen")));
				
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().OpenDebugObjectTreeTab,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.OpenDebugTreeTab")));

		Section.AddSeparator(NAME_None);

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			 PCGEditorCommands.ToggleGraphParams,
			 TAttribute<FText>(),
			 TAttribute<FText>(),
			 FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.OpenGraphParams")));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			 PCGEditorCommands.EditGraphSettings,
			 TAttribute<FText>(),
			 TAttribute<FText>(),
			 FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.GraphSettings")));
	}
}

void FPCGEditor::BindCommands()
{
	const FPCGEditorCommands& PCGEditorCommands = FPCGEditorCommands::Get();

	ToolkitCommands->MapAction(
		PCGEditorCommands.Find,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnFind));

	ToolkitCommands->MapAction(
		PCGEditorCommands.ShowSelectedDetails,
		FExecuteAction::CreateSP(this, &FPCGEditor::OpenDetailsView));

	ToolkitCommands->MapAction(
		PCGEditorCommands.PauseAutoRegeneration,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnPauseAutomaticRegeneration_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPCGEditor::IsAutomaticRegenerationPaused));

	ToolkitCommands->MapAction(
		PCGEditorCommands.ForceGraphRegeneration,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnForceGraphRegeneration_Clicked));

	ToolkitCommands->MapAction(
		PCGEditorCommands.CancelExecution,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnCancelExecution_Clicked),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::IsCurrentlyGenerating));

	// Left on UI as a disabled button if debug object tree tab already open. This is a deliberate
	// hint for 5.4 to help direct users to use the tree.
	ToolkitCommands->MapAction(
		PCGEditorCommands.OpenDebugObjectTreeTab,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnOpenDebugObjectTreeTab_Clicked),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::IsDebugObjectTreeTabClosed));

	ToolkitCommands->MapAction(
		PCGEditorCommands.RunDeterminismGraphTest,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDeterminismGraphTest),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanRunDeterminismGraphTest));

	ToolkitCommands->MapAction(
		PCGEditorCommands.ToggleGraphParams,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnToggleGraphParamsPanel),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPCGEditor::IsToggleGraphParamsToggled));

	ToolkitCommands->MapAction(
		PCGEditorCommands.EditGraphSettings,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnEditGraphSettings),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPCGEditor::IsEditGraphSettingsToggled));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.CollapseNodes,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnCollapseNodesInSubgraph),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanCollapseNodesInSubgraph));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ExportNodes,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnExportNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanExportNodes));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ConvertToStandaloneNodes,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnConvertToStandaloneNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanConvertToStandaloneNodes));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ToggleInspect,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnToggleInspected),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanToggleInspected),
		FGetActionCheckState::CreateSP(this, &FPCGEditor::GetInspectedCheckState));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.RunDeterminismNodeTest,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDeterminismNodeTest),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanRunDeterminismNodeTest));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ToggleEnabled,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnToggleEnabled),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanToggleEnabled),
		FGetActionCheckState::CreateSP(this, &FPCGEditor::GetEnabledCheckState));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ToggleDebug,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnToggleDebug),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanToggleDebug),
		FGetActionCheckState::CreateSP(this, &FPCGEditor::GetDebugCheckState));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.DebugOnlySelected,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDebugOnlySelected),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanToggleDebug));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.DisableDebugOnAllNodes,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDisableDebugOnAllNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanToggleDebug));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.AddSourcePin,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAddDynamicInputPin),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanAddDynamicInputPin));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.RenameNode,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnRenameNode),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanRenameNode));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.SelectNamedRerouteUsages,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnSelectNamedRerouteUsages),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanSelectNamedRerouteUsages));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.SelectNamedRerouteDeclaration,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnSelectNamedRerouteDeclaration),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanSelectNamedRerouteDeclaration));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.JumpToSource,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnJumpToSource));
}

void FPCGEditor::OnFind()
{
	if (TabManager.IsValid() && FindWidget.IsValid())
	{
		TabManager->TryInvokeTab(FPCGEditor_private::FindID);
		FindWidget->FocusForUse();
	}
}

void FPCGEditor::OpenDetailsView()
{
	if (TabManager.IsValid())
	{
		auto InvokeFirstUnlockedTab = [this](bool bVisibleOnly) -> bool
		{
			for (int DetailsViewIndex = 0; DetailsViewIndex < PropertyDetailsWidgets.Num(); ++DetailsViewIndex)
			{
				TSharedPtr<SPCGEditorGraphDetailsView> DetailsView = PropertyDetailsWidgets[DetailsViewIndex];
				if (DetailsView.IsValid() && !DetailsView->IsLocked())
				{
					if (!bVisibleOnly || TabManager->FindExistingLiveTab(FPCGEditor_private::PropertyDetailsID[DetailsViewIndex]))
					{
						TabManager->TryInvokeTab(FPCGEditor_private::PropertyDetailsID[DetailsViewIndex]);
						return true;
					}
				}
			}

			return false;
		};

		if (InvokeFirstUnlockedTab(true) || InvokeFirstUnlockedTab(false))
		{
			return;
		}

		// Default to first if they are all locked
		if (PropertyDetailsWidgets[0].IsValid())
		{
			TabManager->TryInvokeTab(FPCGEditor_private::PropertyDetailsID[0]);
		}
	}
}

void FPCGEditor::OnDetailsViewTabClosed(TSharedRef<SDockTab> DockTab, int Index)
{
	if (!PropertyDetailsWidgets.IsValidIndex(Index))
	{
		return;
	}

	TSharedPtr<SPCGEditorGraphDetailsView> DetailsView = PropertyDetailsWidgets[Index];
	if (DetailsView.IsValid() && DetailsView->IsLocked())
	{
		DetailsView->SetIsLocked(false);
	}
}

void FPCGEditor::OnAttributeListViewTabClosed(TSharedRef<SDockTab> DockTab, int Index)
{
	if (!AttributesWidgets.IsValidIndex(Index))
	{
		return;
	}

	TSharedPtr<SPCGEditorGraphAttributeListView> AttributeListView = AttributesWidgets[Index];
	if (AttributeListView.IsValid())
	{
		if (AttributeListView->IsLocked())
		{
			AttributeListView->SetIsLocked(false);
		}

		UPCGEditorGraphNodeBase* NodeInspected = AttributeListView->GetNodeBeingInspected();
		AttributeListView->SetNodeBeingInspected(nullptr);

		if (NodeInspected)
		{
			bool bIsStillInspectedOnVisibleTabs = false;
			for (int OtherTabIndex = 0; OtherTabIndex < AttributesWidgets.Num(); ++OtherTabIndex)
			{
				TSharedPtr<SPCGEditorGraphAttributeListView> ALV = AttributesWidgets[OtherTabIndex];
				if (ALV.IsValid() && ALV->GetNodeBeingInspected() == NodeInspected && TabManager->FindExistingLiveTab(FPCGEditor_private::AttributesID[OtherTabIndex]))
				{
					bIsStillInspectedOnVisibleTabs = true;
					break;
				}
			}

			if (!bIsStillInspectedOnVisibleTabs)
			{
				NodeInspected->SetInspected(false);

				for (TSharedPtr<SPCGEditorGraphAttributeListView> ALV : AttributesWidgets)
				{
					if (ALV.IsValid() && ALV->GetNodeBeingInspected() == NodeInspected)
					{
						ALV->SetNodeBeingInspected(nullptr);
					}
				}
			}
		}
	}
}

void FPCGEditor::OnViewportViewTabClosed(TSharedRef<SDockTab> DockTab, int Index)
{
	AttributesWidgets[Index]->ResetViewport();
}

void FPCGEditor::OnPauseAutomaticRegeneration_Clicked()
{
	if (!PCGGraphBeingEdited)
	{
		return;
	}

	PCGGraphBeingEdited->ToggleUserPausedNotificationsForEditor();
}

bool FPCGEditor::IsAutomaticRegenerationPaused() const
{
	return PCGGraphBeingEdited && PCGGraphBeingEdited->NotificationsForEditorArePausedByUser();
}

void FPCGEditor::OnForceGraphRegeneration_Clicked()
{
	if (PCGGraphBeingEdited)
	{
		EPCGChangeType ChangeType = EPCGChangeType::Structural;

		FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
		if (ModifierKeys.IsControlDown())
		{
			if (UPCGSubsystem* Subsystem = GetSubsystem())
			{
				Subsystem->FlushCache();
			}

			ChangeType |= EPCGChangeType::GenerationGrid;

			ChangeType |= EPCGChangeType::ShaderSource;
		}

		PCGGraphBeingEdited->ForceNotificationForEditor(ChangeType);
	}
}

void FPCGEditor::OnCancelExecution_Clicked()
{
	UPCGSubsystem* Subsystem = GetSubsystem();
	if (PCGEditorGraph && Subsystem)
	{
		Subsystem->CancelGeneration(PCGEditorGraph->GetPCGGraph());
	}
}

bool FPCGEditor::IsCurrentlyGenerating() const
{
	UPCGSubsystem* Subsystem = GetSubsystem();
	if (PCGGraphBeingEdited && Subsystem)
	{
		return Subsystem->IsGraphCurrentlyExecuting(PCGGraphBeingEdited);
	}

	return false;
}

bool FPCGEditor::IsDebugObjectTreeTabClosed() const
{
	return !TabManager.IsValid() || !TabManager->FindExistingLiveTab(FPCGEditor_private::DebugObjectID).IsValid();
}

void FPCGEditor::OnOpenDebugObjectTreeTab_Clicked()
{
	TabManager->TryInvokeTab(FPCGEditor_private::DebugObjectID);
}

bool FPCGEditor::CanRunDeterminismNodeTest() const
{
	check(GraphEditorWidget.IsValid());

	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		if (Cast<const UPCGEditorGraphNodeBase>(Object) && !Cast<const UPCGEditorGraphNodeInput>(Object) && !Cast<const UPCGEditorGraphNodeOutput>(Object))
		{
			return true;
		}
	}

	return false;
}

void FPCGEditor::OnDeterminismNodeTest() const
{
	check(GraphEditorWidget.IsValid());

	if (!DeterminismWidget.IsValid() || !DeterminismWidget->WidgetIsConstructed())
	{
		return;
	}

	TMap<FName, FTestColumnInfo> TestsConducted;
	DeterminismWidget->ClearItems();
	DeterminismWidget->BuildBaseColumns();

	int64 TestIndex = 0;
	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Gets an appropriate width for each new column
		auto GetSlateTextWidth = [](const FText& Text) -> float
		{
			check(FSlateApplication::Get().GetRenderer());
			const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			// TODO: Verify the below property for this part of the UI
			FSlateFontInfo FontInfo(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"));
			constexpr float Padding = 30.f;
			return Padding + FontMeasure->Measure(Text, FontInfo).X;
		};

		if (!Object->IsA<UPCGEditorGraphNodeInput>() && !Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			if (const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<const UPCGEditorGraphNodeBase>(Object))
			{
				const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
				check(PCGNode && PCGNode->GetSettings());

				TSharedPtr<FDeterminismTestResult> NodeResult = MakeShared<FDeterminismTestResult>();
				NodeResult->Index = TestIndex++;
				NodeResult->TestResultTitle = FName(*PCGNode->GetNodeTitle(EPCGNodeTitleType::ListView).ToString());
				NodeResult->TestResultName = PCGNode->GetName();
				NodeResult->Seed = PCGNode->GetSettings()->GetSeed();

				if (PCGNode->GetSettings()->DeterminismSettings.bNativeTests)
				{
					// If the settings has a native test suite
					if (TFunction<bool()> NativeTestSuite = PCGDeterminismTests::FNativeTestRegistry::GetNativeTestFunction(PCGNode->GetSettings()))
					{
						FName NodeName(PCGNode->GetName());

						bool bSuccess = NativeTestSuite();
						NodeResult->TestResults.Emplace(NodeName, bSuccess ? EDeterminismLevel::Basic : EDeterminismLevel::NoDeterminism);
						NodeResult->AdditionalDetails.Emplace(FString(TEXT("Native test conducted for - ")) + NodeName.ToString());
						NodeResult->bFlagRaised = !bSuccess;

						FText ColumnText = NSLOCTEXT("PCGDeterminism", "NativeTest", "Native Test");
						TestsConducted.FindOrAdd(NodeName, {NodeName, ColumnText, GetSlateTextWidth(ColumnText), HAlign_Center});
					}
					else // There is no native test suite, so run the basic tests
					{
						PCGDeterminismTests::FNodeTestInfo BasicTestInfo = PCGDeterminismTests::Defaults::DeterminismBasicTestInfo;
						PCGDeterminismTests::RunDeterminismTest(PCGNode, *NodeResult, BasicTestInfo);
						TestsConducted.FindOrAdd(BasicTestInfo.TestName, {BasicTestInfo.TestName, BasicTestInfo.TestLabel, BasicTestInfo.TestLabelWidth, HAlign_Center});

						PCGDeterminismTests::FNodeTestInfo OrderIndependenceTestInfo = PCGDeterminismTests::Defaults::DeterminismOrderIndependenceInfo;
						PCGDeterminismTests::RunDeterminismTest(PCGNode, *NodeResult, OrderIndependenceTestInfo);
						TestsConducted.FindOrAdd(OrderIndependenceTestInfo.TestName, {OrderIndependenceTestInfo.TestName, OrderIndependenceTestInfo.TestLabel, OrderIndependenceTestInfo.TestLabelWidth, HAlign_Center});
					}
				}

				// Custom tests
				if (PCGNode->GetSettings()->DeterminismSettings.bUseBlueprintDeterminismTest)
				{
					TSubclassOf<UPCGDeterminismTestBlueprintBase> Blueprint = PCGNode->GetSettings()->DeterminismSettings.DeterminismTestBlueprint;
					Blueprint.GetDefaultObject()->ExecuteTest(PCGNode, *NodeResult);
					FName BlueprintName(Blueprint->GetName());

					FText ColumnText = FText::FromString(Blueprint->GetName());
					TestsConducted.FindOrAdd(BlueprintName, {BlueprintName, ColumnText, GetSlateTextWidth(ColumnText), HAlign_Center});
				}

				DeterminismWidget->AddItem(NodeResult);
			}
		}
	}

	for (const TTuple<FName, FTestColumnInfo>& Test : TestsConducted)
	{
		DeterminismWidget->AddColumn(Test.Value);
	}

	DeterminismWidget->AddDetailsColumn();
	DeterminismWidget->RefreshItems();

	// Give focus to the Determinism Output Tab
	if (TabManager.IsValid())
	{
		TabManager->TryInvokeTab(FPCGEditor_private::DeterminismID);
	}
}

bool FPCGEditor::CanRunDeterminismGraphTest() const
{
	return PCGEditorGraph && PCGComponentBeingInspected.IsValid();
}

void FPCGEditor::OnDeterminismGraphTest() const
{
	check(GraphEditorWidget.IsValid());

	if (!DeterminismWidget.IsValid() || !DeterminismWidget->WidgetIsConstructed() || !PCGGraphBeingEdited || !PCGComponentBeingInspected.IsValid())
	{
		return;
	}

	if (PCGComponentBeingInspected->GetGraph() != PCGGraphBeingEdited)
	{
		// TODO: Should we alert the user more directly or disable this altogether?
		UE_LOG(LogPCGEditor, Warning, TEXT("Running Determinism on a PCG Component with different/no attached PCG Graph"));
	}

	DeterminismWidget->ClearItems();
	DeterminismWidget->BuildBaseColumns();

	FTestColumnInfo ColumnInfo({PCGDeterminismTests::Defaults::GraphResultName, NSLOCTEXT("PCGDeterminism", "Result", "Result"), 120.f, HAlign_Center});
	DeterminismWidget->AddColumn(ColumnInfo);

	TSharedPtr<FDeterminismTestResult> TestResult = MakeShared<FDeterminismTestResult>();
	TestResult->Index = 0;
	TestResult->TestResultTitle = TEXT("Full Graph Test");
	TestResult->TestResultName = PCGGraphBeingEdited->GetName();
	TestResult->Seed = PCGComponentBeingInspected->Seed;

	PCGDeterminismTests::RunDeterminismTest(PCGGraphBeingEdited, PCGComponentBeingInspected.Get(), *TestResult);

	DeterminismWidget->AddItem(TestResult);
	DeterminismWidget->AddDetailsColumn();
	DeterminismWidget->RefreshItems();

	// Give focus to the Determinism Output Tab
	if (TabManager.IsValid())
	{
		TabManager->TryInvokeTab(FPCGEditor_private::DeterminismID);
	}
}

void FPCGEditor::OnEditGraphSettings()
{
	check(GraphEditorWidget)

	// Clear any selected nodes.
	GraphEditorWidget->ClearSelectionSet();

	for (TSharedPtr<SPCGEditorGraphDetailsView> PropertyDetailsWidget : PropertyDetailsWidgets)
	{
		PropertyDetailsWidget->SetObject(PCGGraphBeingEdited);
	}

	OpenDetailsView();
}

bool FPCGEditor::IsEditGraphSettingsToggled() const
{
	if (!TabManager.IsValid())
	{
		return false;
	}

	for (int32 WidgetIndex = 0; WidgetIndex < PropertyDetailsWidgets.Num(); ++WidgetIndex)
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyDetailsWidgets[WidgetIndex]->GetSelectedObjects();
		// The only object selected should be the graph. If there is no details view panel open, leave it disabled.
		if (SelectedObjects.Num() == 1 && SelectedObjects[0] == PCGGraphBeingEdited.Get())
		{
			if (const TSharedPtr<SDockTab>& Tab = TabManager->FindExistingLiveTab(FPCGEditor_private::PropertyDetailsID[WidgetIndex]))
			{
				if (Tab.IsValid() && Tab->IsForeground())
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FPCGEditor::OnToggleGraphParamsPanel() const
{
	if (IsPanelCurrentlyForeground(EPCGEditorPanel::UserParams))
	{
		CloseGraphPanel(EPCGEditorPanel::UserParams);
	}
	else
	{
		BringFocusToPanel(EPCGEditorPanel::UserParams);
	}
}

bool FPCGEditor::IsToggleGraphParamsToggled() const
{
	return IsPanelCurrentlyOpen(EPCGEditorPanel::UserParams);
}

bool FPCGEditor::CanCollapseNodesInSubgraph() const
{
	bool HasPCGNode = false;

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		if (Object->IsA<UPCGEditorGraphNodeBase>())
		{
			if (HasPCGNode)
			{
				return true;
			}

			HasPCGNode = true;
		}
	}

	return false;
}

void FPCGEditor::OnAddDynamicInputPin()
{
	check(GraphEditorWidget.IsValid());

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();

	if (!ensure(SelectedNodes.Num() == 1))
	{
		UE_LOG(LogPCGEditor, Warning, TEXT("Attempting to add new input pin to multiple nodes."));
		return;
	}

	UPCGEditorGraphNodeBase* Node = CastChecked<UPCGEditorGraphNodeBase>(*SelectedNodes.CreateConstIterator());
	Node->OnUserAddDynamicInputPin();
}

bool FPCGEditor::CanAddDynamicInputPin() const
{
	check(GraphEditorWidget.IsValid());

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	if (SelectedNodes.Num() != 1)
	{
		return false;
	}

	const UPCGEditorGraphNodeBase* Node = Cast<const UPCGEditorGraphNodeBase>(*SelectedNodes.CreateConstIterator());
	return (Node && Node->CanUserAddRemoveDynamicInputPins());
}

void FPCGEditor::OnRenameNode()
{
	check(GraphEditorWidget.IsValid());

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();

	if (!ensure(SelectedNodes.Num() == 1))
	{
		UE_LOG(LogPCGEditor, Warning, TEXT("Attempting to rename multiple nodes."));
		return;
	}

	const UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(*SelectedNodes.CreateConstIterator());
	if (SelectedNode != nullptr && SelectedNode->GetCanRenameNode())
	{
		GraphEditorWidget->IsNodeTitleVisible(SelectedNode, true);
	}
}

bool FPCGEditor::CanRenameNode() const
{
	check(GraphEditorWidget.IsValid());

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();

	// You cannot enter renaming mode on multiple nodes at once, since they will not all enter synchronously.
	// Simultaneous editing of multiple InlineEditableTextBlocks may not even be possible with default behavior.
	if (SelectedNodes.Num() != 1)
	{
		return false;
	}

	UObject* SelectedObject = *SelectedNodes.CreateConstIterator();
	if (const UPCGEditorGraphNode* SelectedNode = Cast<UPCGEditorGraphNode>(SelectedObject))
	{
		return SelectedNode->GetCanRenameNode();
	}
	else if (SelectedObject && SelectedObject->IsA<UEdGraphNode_Comment>())
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool FPCGEditor::InternalValidationOnAction()
{
	if (!GraphEditorWidget.IsValid() || PCGEditorGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("GraphEditorWidget or PCGEditorGraph is null, aborting"));
		return false;
	}

	UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
	if (PCGGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("PCGGraph is null, aborting"));
		return false;
	}

	return true;
}

void FPCGEditor::OnSelectNamedRerouteUsages()
{
	if (!InternalValidationOnAction())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();

	if (SelectedNodes.Num() != 1)
	{
		return;
	}

	const UPCGEditorGraphNodeNamedRerouteDeclaration* DeclarationNode = nullptr;

	for (const UObject* Object : SelectedNodes)
	{
		DeclarationNode = Cast<UPCGEditorGraphNodeNamedRerouteDeclaration>(Object);
	}

	if (!DeclarationNode || !DeclarationNode->GetPCGNode())
	{
		return;
	}

	GraphEditorWidget->ClearSelectionSet();

	// Some assumptions below - that only usages are connected to the invisible pin.
	if (const UPCGPin* InvisiblePin = DeclarationNode->GetPCGNode()->GetOutputPin(PCGNamedRerouteConstants::InvisiblePinLabel))
	{
		for (const UPCGEdge* Edge : InvisiblePin->Edges)
		{
			if (const UPCGNode* Usage = Edge->OutputPin->Node)
			{
				GraphEditorWidget->SetNodeSelection(GetEditorNode(Usage), true);
			}
		}
	}

	GraphEditorWidget->ZoomToFit(true);
}

bool FPCGEditor::CanSelectNamedRerouteUsages() const
{
	if (!GraphEditorWidget || GraphEditorWidget->GetSelectedNodes().Num() != 1)
	{
		return false;
	}

	if (auto It = GraphEditorWidget->GetSelectedNodes().CreateConstIterator(); It)
	{
		const UObject* Object = *It;
		return Object && Object->IsA<UPCGEditorGraphNodeNamedRerouteDeclaration>();
	}

	return false;
}

void FPCGEditor::OnSelectNamedRerouteDeclaration()
{
	if (!InternalValidationOnAction())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();

	if (SelectedNodes.Num() != 1)
	{
		return;
	}

	for (const UObject* Object : SelectedNodes)
	{
		const UPCGEditorGraphNodeNamedRerouteUsage* UsageNode = Cast<UPCGEditorGraphNodeNamedRerouteUsage>(Object);

		if (!UsageNode)
		{
			continue;
		}

		GraphEditorWidget->ClearSelectionSet();

		if (!UsageNode->GetPCGNode())
		{
			continue;
		}

		// Find the declaration node that matches the settings in the Usage node.
		if (UPCGNamedRerouteUsageSettings* UsageSettings = Cast<UPCGNamedRerouteUsageSettings>(UsageNode->GetPCGNode()->GetSettings()))
		{
			if (UsageSettings->Declaration && UsageSettings->Declaration->GetOuter() && UsageSettings->Declaration->GetOuter()->IsA<UPCGNode>())
			{
				JumpToNode(Cast<UPCGNode>(UsageSettings->Declaration->GetOuter()));
				break;
			}
		}
	}
}

bool FPCGEditor::CanSelectNamedRerouteDeclaration() const
{
	if (!GraphEditorWidget || GraphEditorWidget->GetSelectedNodes().Num() != 1)
	{
		return false;
	}

	if (auto It = GraphEditorWidget->GetSelectedNodes().CreateConstIterator(); It)
	{
		const UObject* Object = *It;
		return Object && Object->IsA<UPCGEditorGraphNodeNamedRerouteUsage>();
	}

	return false;
}

void FPCGEditor::OnJumpToSource()
{
	if (!InternalValidationOnAction())
	{
		return;
	}

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<const UPCGEditorGraphNodeBase>(Object);
		const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
		const UPCGSettings* Settings = PCGNode ? PCGNode->GetSettings() : nullptr;

		if (Settings)
		{
			JumpToDefinition(Settings->GetClass());
		}
	}
}

FReply FPCGEditor::OnSpawnNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UPCGEditorGraph* InGraph)
{
	return OnSpawnNodeByShortcut(InChord, UE::Slate::CastToVector2f(InPosition), InGraph);
}

FReply FPCGEditor::OnSpawnNodeByShortcut(FInputChord InChord, const FVector2f& InPosition, UPCGEditorGraph* InGraph)
{
	const TSharedPtr<FEdGraphSchemaAction> Action = FPCGEditorSpawnNodeCommands::Get().GetGraphActionByChord(InChord);
	if (Action.IsValid())
	{
		TArray<UEdGraphPin*> DummyPins;
		Action->PerformAction(InGraph, DummyPins, UE::Slate::FDeprecateVector2DParameter(InPosition));
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FActionMenuContent FPCGEditor::OnCreateActionMenuContent(UEdGraph* InGraph, const FVector2f& Location, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnMenuClosed)
{
	TSharedRef<SGraphEditorActionMenu> Menu = SNew(SGraphEditorActionMenu)
		.GraphObj(InGraph)
		.NewNodePosition(Location)
		.DraggedFromPins(InDraggedPins)
		.AutoExpandActionMenu(bAutoExpand)
		.OnClosedCallback(OnMenuClosed)
		.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateLambda(
			[](const FCreateWidgetForActionData* const Data)
			{
				return SNew(SPCGGraphActionWidget, Data);
			}));

	return FActionMenuContent(Menu, Menu->GetFilterTextBox());
}

void FPCGEditor::OnCollapseNodesInSubgraph()
{
	if (!InternalValidationOnAction())
	{
		return;
	}

	UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
	check(PCGGraph);

	// Gather all nodes that will be included in the subgraph, and the extra nodes
	TArray<UPCGNode*> NodesToCollapse;
	TArray<UObject*> ExtraNodesToCollapse;

	check(GraphEditorWidget);
	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object))
		{
			UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
			check(PCGNode);
			NodesToCollapse.Add(PCGNode);
		}
		else if (UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object))
		{
			ExtraNodesToCollapse.Add(GraphNode);
		}
	}

	// If we have at most 1 node to collapse, just exit
	if (NodesToCollapse.Num() <= 1)
	{
		UE_LOG(LogPCGEditor, Warning, TEXT("There were less than 2 PCG nodes selected, abort"));
		return;
	}

	// Create a new subgraph, by creating a new PCGGraph asset.
	TObjectPtr<UPCGGraph> NewPCGGraph = nullptr;

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TObjectPtr<UPCGGraphFactory> Factory = NewObject<UPCGGraphFactory>();
	Factory->bSkipTemplateSelection = true;

	FString NewPackageName;
	FString NewAssetName;
	PCGEditorUtils::GetParentPackagePathAndUniqueName(PCGGraph, LOCTEXT("NewPCGSubgraphAsset", "NewPCGSubgraph").ToString(), NewPackageName, NewAssetName);

	NewPCGGraph = Cast<UPCGGraph>(AssetTools.CreateAssetWithDialog(NewAssetName, NewPackageName, PCGGraph->GetClass(), Factory, "PCGEditor_CollapseInSubgraph"));

	if (NewPCGGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Warning, TEXT("Subgraph asset creation was aborted or failed, abort."));
		return;
	}

	{
		FScopedTransaction Transaction(LOCTEXT("PCGCollapseInSubgraphMessage", "[PCG] Collapse into Subgraph"));
		FText OutFailReason;
		NewPCGGraph = FPCGSubgraphHelpers::CollapseIntoSubgraphWithReason(PCGGraph, NodesToCollapse, ExtraNodesToCollapse, OutFailReason, NewPCGGraph);

		if (NewPCGGraph == nullptr)
		{
			FMessageDialog::Open(EAppMsgType::Ok, OutFailReason, LOCTEXT("PCGCollapseInSubgraphFailed", "PCG Subgraph Collapse Failed"));
			Transaction.Cancel();
			return;
		}

		// Force a refresh
		PCGEditorGraph->ReconstructGraph();
	}

	if (NewPCGGraph)
	{
		// Save the new asset
		UEditorAssetLibrary::SaveLoadedAsset(NewPCGGraph);

		// Notify the widget
		GraphEditorWidget->NotifyGraphChanged();
	}
}

bool FPCGEditor::CanExportNodes() const
{
	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		// Also exclude reroute nodes
		if (Object->IsA<UPCGEditorGraphNodeReroute>() || Object->IsA<UPCGEditorGraphNodeNamedRerouteBase>())
		{
			continue;
		}

		if (Object->IsA<UPCGEditorGraphNodeBase>())
		{
			return true;
		}
	}

	return false;
}

void FPCGEditor::OnExportNodes()
{
	if (!GraphEditorWidget.IsValid() || PCGEditorGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("GraphEditorWidget or PCGEditorGraph is null, aborting"));
		return;
	}

	if (PCGGraphBeingEdited == nullptr)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Editor has no graph loaded, aborting"));
		return;
	}

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		UPCGSettings* Settings = nullptr;

		if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object))
		{
			UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
			check(PCGNode);
			Settings = PCGNode->GetSettings();
		}

		if (!Settings)
		{
			continue;
		}

		// Create new settings asset
		FString NewPackageName;
		FString NewAssetName;
		PCGEditorUtils::GetParentPackagePathAndUniqueName(PCGGraphBeingEdited, LOCTEXT("NewPCGSettingsAsset", "NewPCGSettings").ToString(), NewPackageName, NewAssetName);

		UObject* NewSettings = AssetTools.DuplicateAssetWithDialogAndTitle(NewAssetName, NewPackageName, Settings, NSLOCTEXT("PCGEditor_ExportNodes", "PCGEditor_ExportNodesTitle", "Export Settings As..."));

		if (NewSettings == nullptr)
		{
			UE_LOG(LogPCGEditor, Warning, TEXT("Settings asset creation was aborted or failed, abort."));
			return;
		}
	}
}

void FPCGEditor::OnConvertToStandaloneNodes()
{
	const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorConvertToStandaloneMessage", "PCG Editor: Converting instanced nodes to standalone"), nullptr);

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);
		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		if (UPCGEditorGraphNodeBase* Node = Cast<UPCGEditorGraphNodeBase>(Object))
		{
			if (UPCGNode* PCGNode = Node->GetPCGNode())
			{
				if (PCGNode->IsInstance())
				{
					PCGNode->Modify();

					UPCGSettings* SourceSettings = PCGNode->GetSettings();
					check(SourceSettings);

					UPCGSettings* SettingsCopy = DuplicateObject(SourceSettings, PCGNode);
					SettingsCopy->SetFlags(RF_Transactional);

					PCGNode->SetSettingsInterface(SettingsCopy);
				}
			}

			Node->ReconstructNode();
		}
	}

	// Notify the widget
	if (GraphEditorWidget)
	{
		GraphEditorWidget->NotifyGraphChanged();
	}
}

bool FPCGEditor::CanConvertToStandaloneNodes() const
{
	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		if (const UPCGEditorGraphNodeBase* Node = Cast<const UPCGEditorGraphNodeBase>(Object))
		{
			if (const UPCGNode* PCGNode = Node->GetPCGNode())
			{
				if (PCGNode->IsInstance())
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FPCGEditor::OnToggleInspected()
{
	if (!GraphEditorWidget.IsValid())
	{
		return;
	}

	UEdGraphNode* GraphNode = GraphEditorWidget->GetSingleSelectedNode();
	UPCGEditorGraphNodeBase* PCGGraphNodeBase = Cast<UPCGEditorGraphNodeBase>(GraphNode);

	const UPCGNode* PCGNode = PCGGraphNodeBase ? PCGGraphNodeBase->GetPCGNode() : nullptr;
	const UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

	if (PCGSettingsInterface && !PCGSettingsInterface->CanBeDebugged())
	{
		return;
	}

	TArray<UPCGEditorGraphNodeBase*, TInlineAllocator<4>> InspectedNodesBefore;
	for (TSharedPtr<SPCGEditorGraphAttributeListView> AttributeListView : AttributesWidgets)
	{
		InspectedNodesBefore.Add(AttributeListView->GetNodeBeingInspected());
	}

	bool bIsInspecting = false;

	// If the selected node was previously inspected, stop inspecting it, and unselect it from the attribute list views
	if (InspectedNodesBefore.Contains(PCGGraphNodeBase))
	{
		PCGGraphNodeBase->SetInspected(false);

		for (TSharedPtr<SPCGEditorGraphAttributeListView> AttributeListView : AttributesWidgets)
		{
			if (AttributeListView->GetNodeBeingInspected() == PCGGraphNodeBase)
			{
				AttributeListView->SetNodeBeingInspected(nullptr);
			}
		}
	}
	else
	{
		TArray<UPCGEditorGraphNodeBase*, TInlineAllocator<4>> InspectedNodesAfter;

		for (TSharedPtr<SPCGEditorGraphAttributeListView> AttributeListView : AttributesWidgets)
		{
			if (!AttributeListView->IsLocked())
			{
				AttributeListView->SetNodeBeingInspected(PCGGraphNodeBase);
			}

			InspectedNodesAfter.Add(AttributeListView->GetNodeBeingInspected());
		}

		if (InspectedNodesAfter.Contains(PCGGraphNodeBase))
		{
			PCGGraphNodeBase->SetInspected(true);
			bIsInspecting = true;
		}

		for (UPCGEditorGraphNodeBase* BeforeNode : InspectedNodesBefore)
		{
			if (!InspectedNodesAfter.Contains(BeforeNode) && BeforeNode)
			{
				BeforeNode->SetInspected(false);
			}
		}
	}

	if (bIsInspecting)
	{
		// Summon the first attribute list view that is inspecting this node
		auto InvokeFirstTab = [this, PCGGraphNodeBase](bool bVisibleOnly) -> bool
		{
			for (int AttributeListViewIndex = 0; AttributeListViewIndex < AttributesWidgets.Num(); ++AttributeListViewIndex)
			{
				TSharedPtr<SPCGEditorGraphAttributeListView> AttributeListView = AttributesWidgets[AttributeListViewIndex];
				if (AttributeListView->GetNodeBeingInspected() == PCGGraphNodeBase)
				{
					if (!bVisibleOnly || TabManager->FindExistingLiveTab(FPCGEditor_private::AttributesID[AttributeListViewIndex]))
					{
						GetTabManager()->TryInvokeTab(FPCGEditor_private::AttributesID[AttributeListViewIndex]);
						return true;
					}
				}
			}

			return false;
		};

		const bool bTabSummoned = (InvokeFirstTab(true) || InvokeFirstTab(false));

		// Default to first if they are all locked
		if (!bTabSummoned)
		{
			GetTabManager()->TryInvokeTab(FPCGEditor_private::AttributesID[0]);
		}

		DebugObjectTreeWidget->SetNodeBeingInspected(PCGNode);
	}
	else
	{
		DebugObjectTreeWidget->SetNodeBeingInspected(nullptr);
	}

	// Turn on "inspecting" on graph if we now have at least one inspected node and had none before
	UpdateAfterInspectedStackChanged();
}

bool FPCGEditor::CanToggleInspected() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	const FGraphPanelSelectionSet& SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	if (SelectedNodes.Num() != 1)
	{
		// Can only inspect one node.
		return false;
	}

	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
		if (!PCGEditorGraphNode)
		{
			return false;
		}

		const UPCGSettingsInterface* PCGSettingsInterface = PCGEditorGraphNode->GetPCGNode() ? PCGEditorGraphNode->GetPCGNode()->GetSettingsInterface() : nullptr;
		if (PCGSettingsInterface && PCGSettingsInterface->CanBeDebugged())
		{
			return true;
		}
	}

	return false;
}

ECheckBoxState FPCGEditor::GetInspectedCheckState() const
{
	if (GraphEditorWidget.IsValid())
	{
		const FGraphPanelSelectionSet& SelectedNodes = GraphEditorWidget->GetSelectedNodes();

		if (SelectedNodes.IsEmpty())
		{
			return ECheckBoxState::Unchecked;
		}

		bool bAllEnabled = true;
		bool bAnyEnabled = false;

		for (UObject* Object : SelectedNodes)
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			if (!PCGEditorGraphNode)
			{
				continue;
			}

			bAllEnabled &= PCGEditorGraphNode->GetInspected();
			bAnyEnabled |= PCGEditorGraphNode->GetInspected();
		}

		if (bAllEnabled)
		{
			return ECheckBoxState::Checked;
		}
		else if (bAnyEnabled)
		{
			return ECheckBoxState::Undetermined;
		}
	}

	return ECheckBoxState::Unchecked;
}

void FPCGEditor::OnToggleEnabled()
{
	const ECheckBoxState CheckState = GetEnabledCheckState();
	const bool bNewCheckState = !(CheckState != ECheckBoxState::Unchecked);

	// To prevent the changes on the editor node from being in the transaction, we delay reconstruction.
	TArray<FPCGDeferNodeReconstructScope> DeferredEditorNodes;

	if (GraphEditorWidget.IsValid())
	{
		FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorToggleEnableTransactionMessage", "PCG Editor: Toggle Enable Nodes"), nullptr);

		UPCGGraph* PCGGraph = PCGEditorGraph ? PCGEditorGraph->GetPCGGraph() : nullptr;
		if (!ensure(PCGGraph))
		{
			return;
		}

		PCGGraph->DisableNotificationsForEditor();

		bool bChanged = false;
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGSettingsInterface || !PCGSettingsInterface->CanBeDisabled())
			{
				continue;
			}

			if (PCGSettingsInterface->bEnabled != bNewCheckState)
			{
				DeferredEditorNodes.Emplace(PCGEditorGraphNode);
				PCGSettingsInterface->Modify();
				PCGSettingsInterface->SetEnabled(bNewCheckState);
				bChanged = true;
			}
		}

		PCGGraph->EnableNotificationsForEditor();

		if (bChanged)
		{
			GraphEditorWidget->NotifyGraphChanged();
		}
		else
		{
			Transaction.Cancel();
		}
	}
}

bool FPCGEditor::CanToggleEnabled() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
		const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
		if (!PCGNode)
		{
			continue;
		}

		if (PCGNode->GetSettingsInterface() && PCGNode->GetSettingsInterface()->CanBeDisabled())
		{
			return true;
		}
	}

	// Could not toggle enabled on anything in selection.
	return false;
}

ECheckBoxState FPCGEditor::GetEnabledCheckState() const
{
	if (GraphEditorWidget.IsValid())
	{
		bool bAllEnabled = true;
		bool bAnyEnabled = false;

		for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			const UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGSettingsInterface || !PCGSettingsInterface->CanBeDisabled())
			{
				continue;
			}

			bAllEnabled &= PCGSettingsInterface->bEnabled;
			bAnyEnabled |= PCGSettingsInterface->bEnabled;
		}

		if (bAllEnabled)
		{
			return ECheckBoxState::Checked;
		}
		else if (bAnyEnabled)
		{
			return ECheckBoxState::Undetermined;
		}
	}

	return ECheckBoxState::Unchecked;
}

void FPCGEditor::OnToggleDebug()
{
	const ECheckBoxState CheckState = GetDebugCheckState();
	const bool bNewCheckState = !(CheckState != ECheckBoxState::Unchecked);

	if (GraphEditorWidget.IsValid())
	{
		FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorToggleDebugTransactionMessage", "PCG Editor: Toggle Debug Nodes"), nullptr);

		bool bChanged = false;
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGSettingsInterface || !PCGSettingsInterface->CanBeDebugged())
			{
				continue;
			}

			if (PCGSettingsInterface->bDebug != bNewCheckState)
			{
				PCGSettingsInterface->Modify(/*bAlwaysMarkDirty=*/false);
				PCGSettingsInterface->bDebug = bNewCheckState;
				PCGNode->OnNodeChangedDelegate.Broadcast(PCGNode, EPCGChangeType::Settings);
				bChanged = true;
			}
		}

		if (!bChanged)
		{
			Transaction.Cancel();
		}
	}
}

bool FPCGEditor::CanToggleDebug() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
		const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;

		if (PCGNode && PCGNode->GetSettingsInterface()->CanBeDebugged())
		{
			return true;
		}
	}

	// Could not toggle debug on anything in selection.
	return false;
}

void FPCGEditor::OnDebugOnlySelected()
{
	if (GraphEditorWidget.IsValid() && PCGEditorGraph)
	{
		bool bChanged = false;

		const FGraphPanelSelectionSet& SelectedNodes = GraphEditorWidget->GetSelectedNodes();

		FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorDebugOnlySelectedTransactionMessage", "PCG Editor: Debug only selected nodes"), nullptr);

		bool bAnyNonSelectedNodesDebugged = false;
		bool bAllSelectedNodesDebugged = true;

		// Initial pass - inspect state of selected and non-selected nodes.
		for (const UEdGraphNode* Node : PCGEditorGraph->Nodes)
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Node);
			const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			const UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;
			if (!PCGSettingsInterface)
			{
				continue;
			}

			if (SelectedNodes.Contains(PCGEditorGraphNode))
			{
				bAllSelectedNodesDebugged &= PCGSettingsInterface->bDebug;
			}
			else
			{
				bAnyNonSelectedNodesDebugged |= PCGSettingsInterface->bDebug;
			}
		}

		// The selected nodes should be debugged if any non-selected nodes are being debugged, or if the selected
		// nodes are partially being debugged.
		const bool bTargetDebugState = bAnyNonSelectedNodesDebugged || !bAllSelectedNodesDebugged;

		for (UEdGraphNode* Node : PCGEditorGraph->Nodes)
		{
			UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Node);
			UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGSettingsInterface || !PCGSettingsInterface->CanBeDebugged())
			{
				continue;
			}

			// Selected set to target state, non-selected should not be debugged.
			const bool bShouldBeDebug = SelectedNodes.Contains(PCGEditorGraphNode) ? bTargetDebugState : false;

			if (PCGSettingsInterface->bDebug != bShouldBeDebug)
			{
				PCGSettingsInterface->Modify(/*bAlwaysMarkDirty=*/false);
				PCGSettingsInterface->bDebug = bShouldBeDebug;
				PCGNode->OnNodeChangedDelegate.Broadcast(PCGNode, EPCGChangeType::Settings);
				bChanged = true;
			}
		}

		if (!bChanged)
		{
			Transaction.Cancel();
		}
	}
}

void FPCGEditor::OnDisableDebugOnAllNodes()
{
	if (GraphEditorWidget.IsValid() && PCGEditorGraph)
	{
		bool bChanged = false;
		FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorDisableDebugAllNodesTransactionMessage", "PCG Editor: Disable debug on all nodes"), nullptr);

		for (UEdGraphNode* Node : PCGEditorGraph->Nodes)
		{
			UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Node);
			UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;
			if (!PCGSettingsInterface)
			{
				continue;
			}

			if (PCGSettingsInterface->bDebug)
			{
				PCGSettingsInterface->Modify(/*bAlwaysMarkDirty=*/false);
				PCGSettingsInterface->bDebug = false;
				PCGNode->OnNodeChangedDelegate.Broadcast(PCGNode, EPCGChangeType::Settings);
				bChanged = true;
			}
		}

		if (!bChanged)
		{
			Transaction.Cancel();
		}
	}
}

ECheckBoxState FPCGEditor::GetDebugCheckState() const
{
	if (GraphEditorWidget.IsValid())
	{
		bool bAllDebug = true;
		bool bAnyDebug = false;

		for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			const UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGSettingsInterface || !PCGSettingsInterface->CanBeDebugged())
			{
				continue;
			}

			bAllDebug &= PCGSettingsInterface->bDebug;
			bAnyDebug |= PCGSettingsInterface->bDebug;
		}

		if (bAllDebug)
		{
			return ECheckBoxState::Checked;
		}
		else if (bAnyDebug)
		{
			return ECheckBoxState::Undetermined;
		}
	}

	return ECheckBoxState::Unchecked;
}

void FPCGEditor::SelectAllNodes()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->SelectAllNodes();
	}
}

bool FPCGEditor::CanSelectAllNodes() const
{
	return GraphEditorWidget.IsValid();
}

void FPCGEditor::DeleteSelectedNodes()
{
	if (GraphEditorWidget.IsValid())
	{
		UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
		check(PCGEditorGraph && PCGGraph);

		// DeleteSelectedNodes is called directly from UI command 
		PCGGraph->PrimeGraphCompilationCache();

		bool bChanged = false;

		{
			const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorDeleteTransactionMessage", "PCG Editor: Delete"), nullptr);
			PCGEditorGraph->Modify();
		
			TArray<UPCGNode*> NodesToRemove;

			for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
			{
				if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object))
				{
					if (PCGEditorGraphNode->CanUserDeleteNode())
					{
						UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
						check(PCGNode);

						NodesToRemove.Add(PCGNode);

						PCGEditorGraphNode->DestroyNode();
						bChanged = true;
					}
				}
				else if (UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object))
				{
					if (GraphNode->CanUserDeleteNode())
					{
						GraphNode->DestroyNode();
						bChanged = true;
					}
				}
			}

			if (bChanged)
			{
				// Need to modify the pcg graph so comments are also caught.
				PCGGraph->Modify();
				PCGGraph->RemoveNodes(NodesToRemove);
			}
		}

		if (bChanged)
		{
			GraphEditorWidget->ClearSelectionSet();
			GraphEditorWidget->NotifyGraphChanged();
		}
	}
}

bool FPCGEditor::CanDeleteSelectedNodes() const
{
	if (GraphEditorWidget.IsValid())
	{
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			UEdGraphNode* GraphNode = CastChecked<UEdGraphNode>(Object);

			if (GraphNode->CanUserDeleteNode())
			{
				return true;
			}
		}
	}

	return false;
}

void FPCGEditor::CopySelectedNodes()
{
	if (GraphEditorWidget.IsValid())
	{
		const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();

		//TODO: evaluate creating a clipboard object instead of ownership hack
		for (UObject* SelectedNode : SelectedNodes)
		{
			UEdGraphNode* GraphNode = CastChecked<UEdGraphNode>(SelectedNode);
			GraphNode->PrepareForCopying();
		}

		FString ExportedText;
		FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
		FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

		for (UObject* SelectedNode : SelectedNodes)
		{
			if (UPCGEditorGraphNodeBase* PCGGraphNode = Cast<UPCGEditorGraphNodeBase>(SelectedNode))
			{
				PCGGraphNode->PostCopy();
			}
		}
	}
}

bool FPCGEditor::CanCopySelectedNodes() const
{
	if (GraphEditorWidget.IsValid())
	{
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			if (UEdGraphNode* GraphNode = CastChecked<UEdGraphNode>(Object))
			{
				if (GraphNode->CanDuplicateNode())
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FPCGEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedNodes();
}

bool FPCGEditor::CanCutSelectedNodes() const
{
	return CanCopySelectedNodes() && CanDeleteSelectedNodes();
}

void FPCGEditor::PasteNodes()
{
	if (GraphEditorWidget.IsValid())
	{
		PasteNodesHere(GraphEditorWidget->GetPasteLocation2f());
	}
}

void FPCGEditor::PasteNodesHere(const FVector2D& Location)
{
	if (!GraphEditorWidget.IsValid() || !PCGEditorGraph)
	{
		return;
	}

	const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorPasteTransactionMessage", "PCG Editor: Paste"), nullptr);
	PCGEditorGraph->Modify();

	// Clear the selection set (newly pasted stuff will be selected)
	GraphEditorWidget->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(PCGEditorGraph, TextToImport, /*out*/ PastedNodes);

	//Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2D AvgNodePosition(0.0f, 0.0f);

	// Number of nodes used to calculate AvgNodePosition
	int32 AvgCount = 0;

	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		if (PastedNode)
		{
			AvgNodePosition.X += PastedNode->NodePosX;
			AvgNodePosition.Y += PastedNode->NodePosY;
			++AvgCount;
		}
	}

	if (AvgCount > 0)
	{
		float InvNumNodes = 1.0f / float(AvgCount);
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;
	}

	TArray<UPCGNode*> NodesToPaste;

	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		GraphEditorWidget->SetNodeSelection(PastedNode, true);

		PastedNode->NodePosX = (PastedNode->NodePosX - AvgNodePosition.X) + Location.X;
		PastedNode->NodePosY = (PastedNode->NodePosY - AvgNodePosition.Y) + Location.Y;

		PastedNode->SnapToGrid(SNodePanel::GetSnapGridSize());

		PastedNode->CreateNewGuid();

		UPCGEditorGraphNodeBase* PastedPCGGraphNode = Cast<UPCGEditorGraphNodeBase>(PastedNode);
		if (UPCGNode* PastedPCGNode = PastedPCGGraphNode ? PastedPCGGraphNode->GetPCGNode() : nullptr)
		{
			NodesToPaste.Add(PastedPCGNode);
		}
	}

	// Need to modify the pcg graph so comments are also caught.
	PCGGraphBeingEdited->Modify();
	PCGGraphBeingEdited->AddNodes(NodesToPaste);

	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		UPCGEditorGraphNodeBase* PastedPCGGraphNode = Cast<UPCGEditorGraphNodeBase>(PastedNode);
		if (UPCGNode* PastedPCGNode = PastedPCGGraphNode ? PastedPCGGraphNode->GetPCGNode() : nullptr)
		{
			PastedPCGGraphNode->RebuildAfterPaste();
		}
	}

	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		UPCGEditorGraphNodeBase* PastedPCGGraphNode = Cast<UPCGEditorGraphNodeBase>(PastedNode);
		if (UPCGNode* PastedPCGNode = PastedPCGGraphNode ? PastedPCGGraphNode->GetPCGNode() : nullptr)
		{
			PastedPCGGraphNode->PostPaste();

			if (UPCGSettings* Settings = PastedPCGNode->GetSettings())
			{
				Settings->PostPaste();
			}
		}
	}

	GraphEditorWidget->NotifyGraphChanged();
}

bool FPCGEditor::CanPasteNodes() const
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(PCGEditorGraph, ClipboardContent);
}

void FPCGEditor::DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FPCGEditor::CanDuplicateNodes() const
{
	return CanCopySelectedNodes();
}

void FPCGEditor::OnAlignTop()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignTop();
	}
}

void FPCGEditor::OnAlignMiddle()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignMiddle();
	}
}

void FPCGEditor::OnAlignBottom()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignBottom();
	}
}

void FPCGEditor::OnAlignLeft()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignLeft();
	}
}

void FPCGEditor::OnAlignCenter()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignCenter();
	}
}

void FPCGEditor::OnAlignRight()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignRight();
	}
}

void FPCGEditor::OnStraightenConnections()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnStraightenConnections();
	}
}

void FPCGEditor::OnDistributeNodesH()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnDistributeNodesH();
	}
}

void FPCGEditor::OnDistributeNodesV()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnDistributeNodesV();
	}
}

void FPCGEditor::OnCreateComment()
{
	if (PCGEditorGraph)
	{
		FPCGEditorGraphSchemaAction_NewComment CommentAction;

		TSharedPtr<SGraphEditor> GraphEditorPtr = SGraphEditor::FindGraphEditorForGraph(PCGEditorGraph);
		FVector2f Location = FVector2f::ZeroVector;
		if (GraphEditorPtr)
		{
			Location = GraphEditorPtr->GetPasteLocation2f();
		}

		CommentAction.PerformAction(PCGEditorGraph, nullptr, Location);
	}
}

TSharedRef<SGraphEditor> FPCGEditor::CreateGraphEditorWidget()
{
	GraphEditorCommands = MakeShareable(new FUICommandList);

	// Editing commands
	GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateSP(this, &FPCGEditor::SelectAllNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanSelectAllNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &FPCGEditor::DeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanDeleteSelectedNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &FPCGEditor::CopySelectedNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanCopySelectedNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &FPCGEditor::CutSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanCutSelectedNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &FPCGEditor::PasteNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanPasteNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateSP(this, &FPCGEditor::DuplicateNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanDuplicateNodes));

	// Alignment Commands
	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesTop,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignTop)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesMiddle,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignMiddle)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesBottom,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignBottom)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesLeft,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignLeft)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesCenter,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignCenter)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesRight,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignRight)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().StraightenConnections,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnStraightenConnections)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().CreateComment,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnCreateComment)
	);

	// Distribution Commands
	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().DistributeNodesHorizontally,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDistributeNodesH)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().DistributeNodesVertically,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDistributeNodesV)
	);

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("PCGGraphEditorCornerText", "PCG Graph");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FPCGEditor::OnSelectedNodesChanged);
	InEvents.OnVerifyTextCommit = FOnNodeVerifyTextCommit::CreateSP(this, &FPCGEditor::OnValidateNodeTitle);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FPCGEditor::OnNodeTitleCommitted);
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FPCGEditor::OnNodeDoubleClicked);
	InEvents.OnSpawnNodeByShortcutAtLocation = SGraphEditor::FOnSpawnNodeByShortcutAtLocation::CreateSP(this, &FPCGEditor::OnSpawnNodeByShortcut, PCGEditorGraph);
	InEvents.OnCreateActionMenuAtLocation = SGraphEditor::FOnCreateActionMenuAtLocation::CreateSP(this, &FPCGEditor::OnCreateActionMenuContent);

	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(true)
		.Appearance(AppearanceInfo)
		.GraphToEdit(PCGEditorGraph)
		.GraphEvents(InEvents)
		.ShowGraphStateOverlay(false);
}

void FPCGEditor::OnClose()
{
	if (PCGEditorGraph)
	{
		PCGEditorGraph->OnClose();
	}

	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		LevelEditor->OnMapChanged().RemoveAll(this);
	}

	if (GEngine)
	{
		GEngine->OnLevelActorDeleted().RemoveAll(this);
	}

	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);

	FAssetEditorToolkit::OnClose();

	if (PCGComponentBeingInspected.IsValid())
	{
		if (PCGComponentBeingInspected->GetExecutionState().GetInspection().IsInspecting())
		{
			PCGComponentBeingInspected->GetExecutionState().GetInspection().DisableInspection();
		}
	}

	if (LastValidPCGComponentBeingInspected.IsValid())
	{
		if (LastValidPCGComponentBeingInspected->GetExecutionState().GetInspection().IsInspecting())
		{
			LastValidPCGComponentBeingInspected->GetExecutionState().GetInspection().DisableInspection();
		}
	}

	if (PCGGraphBeingEdited)
	{
		PCGGraphBeingEdited->OnGraphChangedDelegate.RemoveAll(this);
		PCGGraphBeingEdited->OnNodeSourceCompiledDelegate.RemoveAll(this);

		if (PCGGraphBeingEdited->IsInspecting())
		{
			PCGGraphBeingEdited->DisableInspection();
		}

		if (PCGGraphBeingEdited->NotificationsForEditorArePausedByUser())
		{
			PCGGraphBeingEdited->ToggleUserPausedNotificationsForEditor();
		}
	}

	if (GEditor)
	{
		UnregisterDelegatesForWorld(GEditor->GetEditorWorldContext().World());
		UnregisterDelegatesForWorld(GEditor->PlayWorld.Get());
	}
}

void FPCGEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	FAssetEditorToolkit::InitToolMenuContext(MenuContext);

	UPCGEditorMenuContext* Context = NewObject<UPCGEditorMenuContext>();
	Context->PCGEditor = SharedThis(this);
	MenuContext.AddObject(Context);
}

TSharedRef<SPCGEditorGraphNodePalette> FPCGEditor::CreatePaletteWidget()
{
	return SNew(SPCGEditorGraphNodePalette, SharedThis(this));
}

TSharedRef<SPCGEditorGraphDebugObjectTree> FPCGEditor::CreateDebugObjectTreeWidget()
{
	return SNew(SPCGEditorGraphDebugObjectTree, SharedThis(this));
}

TSharedRef<SPCGEditorGraphFind> FPCGEditor::CreateFindWidget()
{
	return SNew(SPCGEditorGraphFind, SharedThis(this));
}

TSharedRef<SPCGEditorGraphAttributeListView> FPCGEditor::CreateAttributesWidget()
{
	return SNew(SPCGEditorGraphAttributeListView, SharedThis(this));
}

TSharedRef<SPCGEditorGraphDeterminismListView> FPCGEditor::CreateDeterminismWidget()
{
	return SNew(SPCGEditorGraphDeterminismListView, SharedThis(this));
}

TSharedRef<SPCGEditorGraphProfilingView> FPCGEditor::CreateProfilingWidget()
{
	return SNew(SPCGEditorGraphProfilingView, SharedThis(this));
}

TSharedRef<SPCGEditorGraphLogView> FPCGEditor::CreateLogWidget()
{
	return SNew(SPCGEditorGraphLogView, SharedThis(this));
}

TSharedRef<SPCGEditorNodeSource> FPCGEditor::CreateNodeSourceWidget()
{
	return SNew(SPCGEditorNodeSource);
}

TSharedRef<SPCGEditorGraphUserParametersView> FPCGEditor::CreateGraphParamsWidget()
{
	return SNew(SPCGEditorGraphUserParametersView, SharedThis(this));
}

TSharedRef<SPCGEditorViewport> FPCGEditor::CreateViewportWidget()
{
	return SNew(SPCGEditorViewport);
}

void FPCGEditor::OnSelectedNodesChanged(const TSet<UObject*>& NewSelection)
{
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;

	if (NewSelection.Num() == 0)
	{
		SelectedObjects.Add(PCGGraphBeingEdited);
	}
	else
	{
		for (UObject* Object : NewSelection)
		{
			if (UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object))
			{
				SelectedObjects.Add(GraphNode);
			}
		}
	}

	for (TSharedPtr<SPCGEditorGraphDetailsView> PropertyDetailsWidget : PropertyDetailsWidgets)
	{
		PropertyDetailsWidget->SetObjects(SelectedObjects, /*bForceRefresh=*/true);
	}

	// Give a single selected node with valid settings to the source editor, or give it null so it can clear the UI.
	UPCGEditorGraphNode* SelectedNode = (NewSelection.Num() == 1) ? Cast<UPCGEditorGraphNode>(*NewSelection.CreateConstIterator()) : nullptr;
	UPCGNode* PCGNode = SelectedNode ? SelectedNode->GetPCGNode() : nullptr;
	SetSourceEditorTargetObject(PCGNode ? PCGNode->GetSettings() : nullptr);
}

void FPCGEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	check(PCGGraphBeingEdited);

	if (NodeBeingChanged)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			FText ErrorText;
			if (OnValidateNodeTitle(NewText, NodeBeingChanged, ErrorText))
			{
				const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorRenameNode", "PCG Editor: Rename Node"), nullptr);

				// Implementation detail: In UPCGEditorGraphNode we only set the title under certain conditions, so it calls Modify() itself.
				// However, UEdGraphNode does not call Modify() on its own, so we should still call it in this case.
				if (!NodeBeingChanged->IsA<UPCGEditorGraphNode>())
				{
					NodeBeingChanged->Modify();
					// Modify the graph as well, as non-pcg editor nodes (like the comment nodes) are serialized in UPCGGraph::ExtraEditorNodes.
					PCGGraphBeingEdited->Modify();
				}

				NodeBeingChanged->OnRenameNode(NewText.ToString());
			}
			else
			{
				UE_LOG(LogPCGEditor, Warning, TEXT("%s"), *FText::Format(LOCTEXT("UnableToRenameNode", "Unable to rename node {0}. Reason: {1}"), NodeBeingChanged->GetNodeTitle(ENodeTitleType::FullTitle), std::move(ErrorText)).ToString());
			}
		}

		if (const UPCGEditorGraphNodeBase* PCGEditorNode = Cast<UPCGEditorGraphNodeBase>(NodeBeingChanged))
		{
			PCGEditorNode->OnNodeChangedDelegate.ExecuteIfBound();
		}
	}
}

void FPCGEditor::OnNodeDoubleClicked(UEdGraphNode* Node)
{
	if (Node != nullptr)
	{
		UObject* Object = Node->GetJumpTargetForDoubleClick();

		// "Normal" node
		if (const UPCGSettings* PCGSettings = Cast<UPCGSettings>(Object))
		{
			// Functions may require the GraphEditorWidget's node selection, so set it manually to be safe.
			GraphEditorWidget->SetNodeSelection(Node, /*bSelect=*/true);

			switch (GetDefault<UPCGEditorSettings>()->NodeDoubleClickAction)
			{
				case EPCGEditorDoubleClickAction::ToggleInspectNode:
					if (CanToggleInspected())
					{
						OnToggleInspected();
					}
					break;
				case EPCGEditorDoubleClickAction::ToggleDebugNode:
					if (CanToggleDebug())
					{
						OnToggleDebug();
					}
					break;
				case EPCGEditorDoubleClickAction::JumpToSourceFile:
					JumpToDefinition(PCGSettings->GetClass());
					break;
				case EPCGEditorDoubleClickAction::DoNothing: // fall-through
				default:
					break;
			}
		}
		else // Special options with non-UPCGSettings based targets.
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<const UPCGEditorGraphNodeBase>(Node);
			const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;

			FPCGStack StackToInspect;

			// If we're inspecting, we'll try to find a match in the stacks for subgraphs instead of relying on the static/template subgraph
			if (GetStackBeingInspected() && DebugObjectTreeWidget)
			{
				if (DebugObjectTreeWidget->GetFirstStackFromSelection(PCGNode, /*Graph=*/nullptr, StackToInspect))
				{
					Object = const_cast<UPCGGraph*>(StackToInspect.GetGraphForCurrentFrame());
				}
			}

			if (Object)
			{
				// Open other editor...
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Object);
				IAssetEditorInstance* EditorInstance = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Object, /*bFocusIfOpen*/true);

				FPCGEditor* OtherPCGEditor = static_cast<FPCGEditor*>(EditorInstance);
				if (OtherPCGEditor && !StackToInspect.GetStackFrames().IsEmpty())
				{
					OtherPCGEditor->SetStackBeingInspectedFromAnotherEditor(StackToInspect);
				}
			}
		}
	}
}

void FPCGEditor::JumpToDefinition(const UClass* Class) const
{
	if (ensure(GUnrealEd) && GUnrealEd->GetUnrealEdOptions()->IsCPPAllowed())
	{
		FSourceCodeNavigation::NavigateToClass(Class);
	}
}

void FPCGEditor::OnComponentUnregistered(UPCGComponent* Component)
{
	// Refresh the debug object tree to avoid stale entries from components that have been unregistered.
	if (!Component || Component->GetGraph() == PCGGraphBeingEdited)
	{
		DebugObjectTreeWidget->RequestRefresh();
	}

	if (IPCGEditorModule* PCGEditorModule = IPCGEditorModule::Get())
	{
		PCGEditorModule->GetNodeVisualLogsMutable().ClearLogs(Component);
	}
}

void FPCGEditor::OnComponentGenerationDone(UPCGSubsystem* Subsystem, UPCGComponent* Component, EPCGGenerationStatus Status)
{
	// We want to refresh if the component that is done generating has generated the current graph being edited,
	// or if it is the root of the current stack being inspected (for subgraphs to also be refreshed).
	// If we don't have a component, we refresh nonetheless.
	bool bShouldRefresh = !Component || StackBeingInspected.GetRootComponent() == Component || Component->GetGraph() == PCGGraphBeingEdited;

	// Additionally, if we are not inspecting but the component that's done executing contains this graph, then we should also update.
	IPCGEditorModule* PCGEditorModule = IPCGEditorModule::Get();
	if (!bShouldRefresh && PCGComponentBeingInspected.IsNull() && PCGGraphBeingEdited && PCGEditorModule)
	{
		TArray<FPCGStackSharedPtr> ExecutedStacksContainingThisGraph = PCGEditorModule->GetExecutedStacksPtrs(Component, PCGGraphBeingEdited);
		bShouldRefresh |= !ExecutedStacksContainingThisGraph.IsEmpty();
	}

	if (!bShouldRefresh)
	{
		return;
	}

	OnComponentGenerated(Component);

	const bool CacheDebuggingEnabled = Subsystem && Subsystem->IsGraphCacheDebuggingEnabled();

	// Refresh nodes to report any errors/warnings, and to display culling state after execution.
	check(PCGEditorGraph);
	for (UEdGraphNode* Node : PCGEditorGraph->Nodes)
	{
		if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Node))
		{
			// If we are debugging the graph cache then we need to refresh the cache count displayed in the title after every generation.
			EPCGChangeType ChangeType = CacheDebuggingEnabled ? EPCGChangeType::Cosmetic : EPCGChangeType::None;

			ChangeType |= PCGEditorGraphNode->UpdateErrorsAndWarnings();
			ChangeType |= PCGEditorGraphNode->UpdateStructuralVisualization(PCGComponentBeingInspected.Get(), &StackBeingInspected);
			ChangeType |= PCGEditorGraphNode->UpdateGPUVisualization(PCGComponentBeingInspected.Get(), &StackBeingInspected);

			if (ChangeType != EPCGChangeType::None)
			{
				PCGEditorGraphNode->ReconstructNode();
			}
		}
	}
}

UPCGSubsystem* FPCGEditor::GetSubsystem()
{
	UWorld* World = (GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr);
	return UPCGSubsystem::GetInstance(World);
}

void FPCGEditor::RegisterDelegatesForWorld(UWorld* World)
{
	UnregisterDelegatesForWorld(World);

	if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(World))
	{
		Subsystem->OnPCGComponentUnregistered.AddRaw(this, &FPCGEditor::OnComponentUnregistered);
		Subsystem->OnPCGComponentGenerationDone.AddRaw(this, &FPCGEditor::OnComponentGenerationDone);
	}
}

void FPCGEditor::UnregisterDelegatesForWorld(UWorld* World)
{
	if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(World))
	{
		Subsystem->OnPCGComponentUnregistered.RemoveAll(this);
		Subsystem->OnPCGComponentGenerationDone.RemoveAll(this);
	}
}

void FPCGEditor::OnGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType)
{
	if (!!(ChangeType & EPCGChangeType::ShaderSource))
	{
		// Flush the shader file cache in case we are editing engine or data interface shaders.
		// We could make the user do this manually, but that makes iterating on data interfaces really painful.
		FlushShaderFileCache();
	}

	if (!!(ChangeType & EPCGChangeType::GraphCustomization))
	{
		if (PaletteWidget)
		{
			PaletteWidget->RequestRefresh();
		}		
	}

	if (!!(ChangeType & EPCGChangeType::Edge))
	{
		for (TSharedPtr<SPCGEditorGraphDetailsView> Widget : PropertyDetailsWidgets)
		{
			if (Widget.IsValid() && Widget->GetVisibility() == EVisibility::Visible)
			{
				const TSharedPtr<IDetailsView> DetailsViewPtr = Widget->GetDetailsView();
				DetailsViewPtr->ForceRefresh();
			}
		}
	}
}

void FPCGEditor::OnNodeSourceCompiled(const UPCGNode* InNode, const FPCGCompilerDiagnostics& InDiagnostics)
{
	check(NodeSourceWidget);

	const UPCGSettings* Settings = InNode ? InNode->GetSettings() : nullptr;
	if (Settings && NodeSourceWidget->GetTextProviderObject() == Settings)
	{
		NodeSourceWidget->OnDiagnosticsUpdated(InDiagnostics);
	}
}

void FPCGEditor::OnMapChanged(UWorld* InWorld, EMapChangeType InMapChangedType)
{
	if (InMapChangedType != EMapChangeType::SaveMap)
	{
		RefreshViewsOnLevelChange();

		// Subsystem has been torn down and rebuilt.
		if (GEditor)
		{
			RegisterDelegatesForWorld(GEditor->GetEditorWorldContext().World());
			RegisterDelegatesForWorld(GEditor->PlayWorld.Get());
		}
	}
}

void FPCGEditor::OnPostPIEStarted(bool bIsSimulating)
{
	RegisterDelegatesForWorld(GEditor ? GEditor->PlayWorld.Get() : nullptr);
}

void FPCGEditor::OnEndPIE(bool bIsSimulating)
{
	UnregisterDelegatesForWorld(GEditor ? GEditor->PlayWorld.Get() : nullptr);
}

void FPCGEditor::OnLevelActorDeleted(AActor* InActor)
{
	// Forward call as this makes an effort to retain the selection if the selected component has not been deleted.
	if (DebugObjectTreeWidget.IsValid())
	{
		DebugObjectTreeWidget->RequestRefresh();
	}
}

void FPCGEditor::RefreshViewsOnLevelChange()
{
	if (DebugObjectTreeWidget.IsValid())
	{
		DebugObjectTreeWidget->RequestRefresh();
	}

	for (TSharedPtr<SPCGEditorGraphAttributeListView>& AttributeWidget : AttributesWidgets)
	{
		if (AttributeWidget.IsValid())
		{
			AttributeWidget->RequestRefresh();
		}
	}
}

TSharedRef<FTabManager::FLayout> FPCGEditor::GetDefaultLayout()
{
	return FTabManager::NewLayout("Standalone_PCGGraphEditor_DefaultLayout_v1.0")
		->AddArea // Main PCG Graph Editor Area
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split // Top Section - Graph, Data Viewport, HLSL Source Editor, and Details View
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.65f)
				->Split // Graph Palette
				(
					FTabManager::NewStack()
					->AddTab(FPCGEditor_private::PaletteID, ETabState::SidebarTab, ESidebarLocation::Left, /*SidebarSizeCoefficient=*/0.13f)
				)
				->Split // Data Viewport/HLSL Source Editor
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(FPCGEditor_private::ViewportID[0], ETabState::OpenedTab)
					->AddTab(FPCGEditor_private::HLSLSourceID, ETabState::OpenedTab)
					->SetForegroundTab(FPCGEditor_private::ViewportID[0])
				)
				->Split // Node Graph
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->AddTab(FPCGEditor_private::GraphEditorID, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
				->Split // Details View
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(FPCGEditor_private::PropertyDetailsID[0], ETabState::OpenedTab)
				)
			)
			->Split // Bottom Section - Debug/Params and ALV
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.35f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(FPCGEditor_private::DebugObjectID, ETabState::OpenedTab)
					->AddTab(FPCGEditor_private::UserParamsID, ETabState::OpenedTab)
					->SetForegroundTab(FPCGEditor_private::DebugObjectID)
				)
				->Split // ALV, Profiling, Find, Determinism
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell(false)
					->AddTab(FPCGEditor_private::AttributesID[0], ETabState::OpenedTab)
					->AddTab(FPCGEditor_private::ProfilingID, ETabState::OpenedTab)
					->AddTab(FPCGEditor_private::FindID, ETabState::OpenedTab)
					->AddTab(FPCGEditor_private::DeterminismID, ETabState::ClosedTab)
					->SetForegroundTab(FPCGEditor_private::AttributesID[0])
				)
			)
		);
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_GraphEditor(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGGraphTitle", "Graph"))
		.TabColorScale(GetTabColorScale())
		[
			GraphEditorWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_PropertyDetails(const FSpawnTabArgs& Args, int PropertyDetailsIndex)
{
	TAttribute<FText> Label = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &FPCGEditor::GetDetailsTabLabel, PropertyDetailsIndex));
	TSharedPtr<SPCGEditorGraphDetailsView> DetailsView = PropertyDetailsWidgets[PropertyDetailsIndex];

	return SNew(SDockTab)
		.Label(Label)
		.OnTabClosed_Raw(this, &FPCGEditor::OnDetailsViewTabClosed, PropertyDetailsIndex)
		.TabColorScale(GetTabColorScale())
		[
			DetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Palette(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGPaletteTitle", "Palette"))
		.TabColorScale(GetTabColorScale())
		[
			PaletteWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_DebugObjectTree(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGDebugObjectTitle", "Debug Object"))
		.TabColorScale(GetTabColorScale())
		[
			DebugObjectTreeWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Attributes(const FSpawnTabArgs& Args, int AttributesIndex)
{
	TAttribute<FText> Label = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &FPCGEditor::GetAttributesTabLabel, AttributesIndex));

	return SNew(SDockTab)
		.Label(Label)
		.OnTabClosed_Raw(this, &FPCGEditor::OnAttributeListViewTabClosed, AttributesIndex)
		.TabColorScale(GetTabColorScale())
		[
			AttributesWidgets[AttributesIndex].ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Find(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGFindTitle", "Find"))
		.TabColorScale(GetTabColorScale())
		[
			FindWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Determinism(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGDeterminismTitle", "Determinism"))
		.TabColorScale(GetTabColorScale())
		[
			DeterminismWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Profiling(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGProfilingTitle", "Profiling"))
		.TabColorScale(GetTabColorScale())
		[
			ProfilingWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Log(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGLogTitle", "Log Capture"))
		.TabColorScale(GetTabColorScale())
		[
			LogWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_NodeSource(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGHLSLSourceTitle", "HLSL Source"))
		.TabColorScale(GetTabColorScale())
		[
			NodeSourceWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_UserParams(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(FPCGEditor_private::UserParamsTabName)
		.TabColorScale(GetTabColorScale())
		[
			UserParamsWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args, int ViewportIndex)
{
	TAttribute<FText> Label = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &FPCGEditor::GetViewportTabLabel, ViewportIndex));
	
	AttributesWidgets[ViewportIndex]->RequestViewportRefresh();

	return SNew(SDockTab)
		.Label(Label)
		.OnTabClosed_Raw(this, &FPCGEditor::OnViewportViewTabClosed, ViewportIndex)
		.TabColorScale(GetTabColorScale())
		[
			AttributesWidgets[ViewportIndex]->GetViewportWidget().ToSharedRef()
		];
}

FText FPCGEditor::GetDetailsTabLabel(int DetailsIndex)
{
	if (DetailsIndex == 0)
	{
		return LOCTEXT("PCGDetailsTitle", "Details");
	}
	else
	{
		return FText::Format(LOCTEXT("PCGDetailsTitle_Multi", "Details {0}"), DetailsIndex + 1);
	}
}

FText FPCGEditor::GetDetailsViewObjectName(int DetailsIndex)
{
	return LOCTEXT("PCGDetailsName", "This is a node name placeholder");
}

FText FPCGEditor::GetAttributesTabLabel(int AttributesIndex)
{
	if (AttributesIndex == 0)
	{
		return LOCTEXT("PCGAttributesTitle", "Attributes");
	}
	else
	{
		return FText::Format(LOCTEXT("PCGAttributesTitle_Multi", "Attributes {0}"), AttributesIndex + 1);
	}
}

FText FPCGEditor::GetViewportTabLabel(int ViewportIndex)
{
	if (ViewportIndex == 0)
	{
		return LOCTEXT("PCGViewportTitle", "Data Viewport");
	}
	else
	{
		return FText::Format(LOCTEXT("PCGViewportTitle_Multi", "Data Viewport {0}"), ViewportIndex + 1);
	}
}

#undef LOCTEXT_NAMESPACE