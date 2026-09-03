// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigVMNewEditor.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintEditorSettings.h"
#include "BlueprintEditorTabs.h"
#include "IMessageLogListing.h"
#include "K2Node_Composite.h"
#include "RigVMSettings.h"
#include "SGraphPanel.h"
#include "SRigVMGraphTitleBar.h"
#include "UnrealEdGlobals.h"
#include "Editor/SRigVMActionMenu.h"
#include "Editor/UnrealEdEngine.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/DebuggerCommands.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "UObject/MetaData.h"
#include "Widgets/Docking/SDockTab.h"
#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Stats/StatsHierarchical.h"
#include "Logging/MessageLog.h"
#include "GraphEditor.h"
#include "GraphEditorActions.h"
#include "MessageLogModule.h"
#include "RigVMBlueprint.h"
#include "RigVMEditorCommands.h"
#include "Editor/RigVMFindReferences.h"
#include "Editor/RigVMCompilerResultsTabSummoner.h"
#include "Editor/RigVMDetailsInspectorTabSummoner.h"
#include "Editor/RigVMFindReferencesTabSummoner.h"
#include "Editor/RigVMGraphEditorSummoner.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Editor/SRigVMDetailsInspector.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/UObjectToken.h"
#include "Widgets/SRigVMEditorSelectedDebugObjectWidget.h"


#define LOCTEXT_NAMESPACE "RigVMNewEditor"

const FName FRigVMNewEditor::SelectionState_GraphExplorer() {	static FName State = (TEXT("GraphExplorer")); return State; };
const FName FRigVMNewEditor::SelectionState_Graph() {			static FName State = (TEXT("Graph")); return State; };
const FName FRigVMNewEditor::SelectionState_ClassSettings() {	static FName State = (TEXT("ClassSettings")); return State; };
const FName FRigVMNewEditor::SelectionState_ClassDefaults() {	static FName State = (TEXT("ClassDefaults")); return State; };

namespace RigVMNewEditorImpl
{
	static const float InstructionFadeDuration = 0.5f;
	
	/**
	 * Searches through a blueprint, looking for the most severe error'ing node.
	 * 
	 * @param  Blueprint	The blueprint to search through.
	 * @param  Severity		Defines the severity of the error/warning to search for.
	 * @return The first node found with the specified error.
	 */
	static UEdGraphNode* FindNodeWithError(UBlueprint* Blueprint, EMessageSeverity::Type Severity = EMessageSeverity::Error);

	/**
	 * Searches through an error log, looking for the most severe error'ing node.
	 * 
	 * @param  ErrorLog		The error log you want to search through.
	 * @param  Severity		Defines the severity of the error/warning to search for.
	 * @return The first node found with the specified error.
	 */
	static UEdGraphNode* FindNodeWithError(FCompilerResultsLog const& ErrorLog, EMessageSeverity::Type Severity = EMessageSeverity::Error);

	/**
	 * Utility function that will check to see if the specified graph has any
	 * nodes that were default, pre-placed, in the graph.
	 *
	 * @param  InGraph  The graph to check.
	 * @return True if the graph has any pre-placed nodes, otherwise false.
	 */
	static bool GraphHasDefaultNode(UEdGraph const* InGraph);

	/**
	 * Utility function that will check to see if the specified graph has any 
	 * nodes other than those that come default, pre-placed, in the graph.
	 *
	 * @param  InGraph  The graph to check.
	 * @return True if the graph has any nodes added by the user, otherwise false.
	 */
	static bool GraphHasUserPlacedNodes(UEdGraph const* InGraph);
}

static UEdGraphNode* RigVMNewEditorImpl::FindNodeWithError(UBlueprint* Blueprint, EMessageSeverity::Type Severity/* = EMessageSeverity::Error*/)
{
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	UEdGraphNode* ChoiceNode = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->bHasCompilerMessage && !Node->ErrorMsg.IsEmpty() && (Node->ErrorType <= Severity))
			{
				if ((ChoiceNode == nullptr) || (ChoiceNode->ErrorType > Node->ErrorType))
				{
					ChoiceNode = Node;
					if (ChoiceNode->ErrorType == 0)
					{
						break;
					}
				}
			}
		}
	}
	return ChoiceNode;
}

static UEdGraphNode* RigVMNewEditorImpl::FindNodeWithError(FCompilerResultsLog const& ErrorLog, EMessageSeverity::Type Severity/* = EMessageSeverity::Error*/)
{
	UEdGraphNode* ChoiceNode = nullptr;
	for (TWeakObjectPtr<UEdGraphNode> NodePtr : ErrorLog.AnnotatedNodes)
	{
		UEdGraphNode* Node = NodePtr.Get();
		if ((Node != nullptr) && (Node->ErrorType <= Severity))
		{
			if ((ChoiceNode == nullptr) || (Node->ErrorType < ChoiceNode->ErrorType))
			{
				ChoiceNode = Node;
				if (ChoiceNode->ErrorType == 0)
				{
					break;
				}
			}
		}
	}

	return ChoiceNode;
}

static bool RigVMNewEditorImpl::GraphHasDefaultNode(UEdGraph const* InGraph)
{
	bool bHasDefaultNodes = false;

	for (UEdGraphNode const* Node : InGraph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}

		if (Node->GetPackage()->GetMetaData().HasValue(Node, FNodeMetadata::DefaultGraphNode) && Node->IsNodeEnabled())
		{
			bHasDefaultNodes = true;
			break;
		}
	}

	return bHasDefaultNodes;
}

static bool RigVMNewEditorImpl::GraphHasUserPlacedNodes(UEdGraph const* InGraph)
{
	bool bHasUserPlacedNodes = false;

	for (UEdGraphNode const* Node : InGraph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}

		if (!Node->GetPackage()->GetMetaData().HasValue(Node, FNodeMetadata::DefaultGraphNode))
		{
			bHasUserPlacedNodes = true;
			break;
		}
	}

	return bHasUserPlacedNodes;
}

FRigVMNewEditor::FRigVMNewEditor()
	: FRigVMEditorBase()
	, bIsActionMenuContextSensitive(true)
{
	DocumentManager = MakeShareable(new FDocumentTracker);
}

void FRigVMNewEditor::OnClose()
{
	FRigVMEditorBase::UnbindEditor();
	FWorkflowCentricApplication::OnClose();
}

void FRigVMNewEditor::InitAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const FName AppIdentifier,
                                      const TSharedRef<FTabManager::FLayout>& StandaloneDefaultLayout, const bool bCreateDefaultStandaloneMenu, const bool bCreateDefaultToolbar,
                                      const TArray<UObject*>& ObjectsToEdit, const bool bInIsToolbarFocusable, const bool bInUseSmallToolbarIcons,
                                      const TOptional<EAssetOpenMethod>& InOpenMethod)
{
	FWorkflowCentricApplication::InitAssetEditor(Mode, InitToolkitHost, AppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit, bInIsToolbarFocusable, bInUseSmallToolbarIcons, InOpenMethod);
}

void FRigVMNewEditor::CommonInitialization(const TArray<UBlueprint*>& InitBlueprints, bool bShouldOpenInDefaultsMode)
{
	TSharedPtr<FRigVMNewEditor> ThisPtr = SharedThis(this);

	// @todo TabManagement
	DocumentManager->Initialize(ThisPtr);

	// Register the document factories
	{
		// TODO sara-s
		//DocumentManager->RegisterDocumentFactory(MakeShareable(new FTimelineEditorSummoner(ThisPtr)));

		TSharedRef<FRigVMNewEditor> SharedRef = StaticCastSharedRef<FRigVMNewEditor>(SharedThis(this));
		TSharedRef<FDocumentTabFactory> GraphEditorFactory = MakeShareable(new FRigVMGraphEditorSummoner(ThisPtr,
			FRigVMGraphEditorSummoner::FOnCreateGraphEditorWidget::CreateSP(SharedRef, &FRigVMNewEditor::CreateGraphEditorWidget)
			));

		// Also store off a reference to the grapheditor factory so we can find all the tabs spawned by it later.
		GraphEditorTabFactoryPtr = GraphEditorFactory;
		DocumentManager->RegisterDocumentFactory(GraphEditorFactory);
	}

	// Create a namespace helper to keep track of imports for all BPs being edited.
	//ImportedNamespaceHelper = MakeShared<FBlueprintNamespaceHelper>();

	// Add each Blueprint instance to be edited into the namespace helper's context.
	// for (const UBlueprint* BP : InitBlueprints)
	// {
	// 	ImportedNamespaceHelper->AddBlueprint(BP);
	// }

	// Create imported namespace type filters for value editing.
	// ImportedClassViewerFilter = MakeShared<BlueprintEditorImpl::FImportedClassViewerFilterProxy>(ImportedNamespaceHelper->GetClassViewerFilter());
	// ImportedPinTypeSelectorFilter = MakeShared<BlueprintEditorImpl::FImportedPinTypeSelectorFilterProxy>(ImportedNamespaceHelper->GetPinTypeSelectorFilter());
	// PermissionsPinTypeSelectorFilter = MakeShared<BlueprintEditorImpl::FPermissionsPinTypeSelectorFilter>(InitBlueprints);

	// Make sure we know when tabs become active to update details tab
	//OnActiveTabChangedDelegateHandle = FGlobalTabmanager::Get()->OnActiveTabChanged_Subscribe( FOnActiveTabChanged::FDelegate::CreateRaw(this, &FBlueprintEditor::OnActiveTabChanged) );

	if (InitBlueprints.Num() == 1)
	{
		if (!bShouldOpenInDefaultsMode)
		{
			// Load blueprint libraries
			// if (ShouldLoadBPLibrariesFromAssetRegistry())
			// {
			// 	LoadLibrariesFromAssetRegistry();
			// }

			// Init the action DB for the context menu/palette if not already constructed
			FBlueprintActionDatabase::Get();
		}

		//FLoadObjectsFromAssetRegistryHelper::Load<UUserDefinedEnum>(UserDefinedEnumerators);

		UBlueprint* InitBlueprint = InitBlueprints[0];

		// Update the blueprint if required
		EBlueprintStatus OldStatus = InitBlueprint->Status;
		//EnsureBlueprintIsUpToDate(InitBlueprint); // TODO sara-s
		UPackage* BpPackage = InitBlueprint->GetOutermost();
		bBlueprintModifiedOnOpen = (InitBlueprint->Status != OldStatus) && !BpPackage->HasAnyPackageFlags(PKG_NewlyCreated);

		// Flag the blueprint as having been opened
		InitBlueprint->bIsNewlyCreated = false;

		// When the blueprint that we are observing changes, it will notify this wrapper widget.
		InitBlueprint->OnChanged().AddSP(this, &FRigVMNewEditor::OnBlueprintChanged); // TODO sara-s
		// InitBlueprint->OnCompiled().AddSP(this, &FRigVMNewEditor::OnBlueprintCompiled); // TODO sara-s
		InitBlueprint->OnSetObjectBeingDebugged().AddSP(this, &FRigVMNewEditor::HandleSetObjectBeingDebugged);
	}

	bWasOpenedInDefaultsMode = bShouldOpenInDefaultsMode;

	CreateDefaultTabContents(InitBlueprints);

	// FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddSP(this, &FRigVMNewEditor::OnPreObjectPropertyChanged); // TODO sara-s
	// FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &FRigVMNewEditor::OnPostObjectPropertyChanged); // TODO sara-s

	// FKismetEditorUtilities::OnBlueprintUnloaded.AddSP(this, &FRigVMNewEditor::OnBlueprintUnloaded); // TODO sara-s

	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}
}

void FRigVMNewEditor::OnBlueprintChanged(UBlueprint* InBlueprint)
{
	if (InBlueprint)
	{
		// Notify that the blueprint has been changed (update Content browser, etc)
		InBlueprint->PostEditChange();

		// Call PostEditChange() on any Actors that are based on this Blueprint 
		FBlueprintEditorUtils::PostEditChangeBlueprintActors(InBlueprint);

		// Refresh the graphs
		//TODO sara-s
		ERefreshRigVMEditorReason::Type Reason = ERefreshRigVMEditorReason::UnknownReason;// = bIsJustBeingCompiled ? ERefreshBlueprintEditorReason::BlueprintCompiled : ERefreshBlueprintEditorReason::UnknownReason;
		ForceEditorRefresh(Reason);

		// In case objects were deleted, which should close the tab
		if (GetCurrentMode() == FRigVMNewEditorApplicationModes::StandardRigVMEditorMode())
		{
			SaveEditedObjectState();
		}
	}
}

void FRigVMNewEditor::SaveEditedObjectState()
{
	check(IsEditingSingleBlueprint());

	// Clear currently edited documents
	GetBlueprintObj()->LastEditedDocuments.Empty();

	// Ask all open documents to save their state, which will update LastEditedDocuments
	DocumentManager->SaveAllState();
}

void FRigVMNewEditor::SetCurrentMode(FName NewMode)
{
	// Clear the selection state when the mode changes.
	SetUISelectionState(NAME_None);

	// TODO sara-s: Replicate what blueprint editor is doing
	//OnModeSetData.Broadcast( NewMode );
	
	FWorkflowCentricApplication::SetCurrentMode(NewMode);
}

void FRigVMNewEditor::PostLayoutBlueprintEditorInitialization()
{
	// TODO sara-s: Replicate what blueprint editor is doing
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		// Refresh the graphs
		ForceEditorRefresh();
		
		// EnsureBlueprintIsUpToDate may have updated the blueprint so show notifications to user.
		if (bBlueprintModifiedOnOpen)
		{
			bBlueprintModifiedOnOpen = false;

			if (FocusedGraphEdPtr.IsValid())
			{
				FNotificationInfo Info( NSLOCTEXT("RigVM", "Blueprint Modified", "Blueprint requires updating. Please resave.") );
				Info.Image = FAppStyle::GetBrush(TEXT("Icons.Info"));
				Info.bFireAndForget = true;
				Info.bUseSuccessFailIcons = false;
				Info.ExpireDuration = 5.0f;

				FocusedGraphEdPtr.Pin()->AddNotification(Info, true);
			}

			// Fire log message
			FString BlueprintName;
			Blueprint->GetName(BlueprintName);

			FFormatNamedArguments Args;
			Args.Add( TEXT("BlueprintName"), FText::FromString( BlueprintName ) );
			LogSimpleMessage( FText::Format( LOCTEXT("Blueprint Modified Long", "Blueprint \"{BlueprintName}\" was updated to fix issues detected on load. Please resave."), Args ) );
		}

		// Determine if the current "mode" supports invoking the Compiler Results tab.
		const bool bCanInvokeCompilerResultsTab = TabManager->HasTabSpawner(FRigVMCompilerResultsTabSummoner::TabID());

		// If we have a warning/error, open output log if the current mode allows us to invoke it.
		if (bCanInvokeCompilerResultsTab)
		{
			TabManager->TryInvokeTab(FRigVMCompilerResultsTabSummoner::TabID());
		}
		else
		{
			// Toolkit modes that don't include this tab may have been incorrectly saved with layout information for restoring it
			// as an "unrecognized" tab, due to having previously invoked it above without checking to see if the layout can open
			// it first. To correct this, we check if the tab was restored from a saved layout here, and close it if not supported.
			TSharedPtr<SDockTab> TabPtr = TabManager->FindExistingLiveTab(FRigVMCompilerResultsTabSummoner::TabID());
			if (TabPtr.IsValid() && !bCanInvokeCompilerResultsTab)
			{
				TabPtr->RequestCloseTab();
			}
		}
	}
}

TSharedPtr<SGraphEditor> FRigVMNewEditor::OpenGraphAndBringToFront(UEdGraph* Graph, bool bSetFocus)
{
	if (!IsValid(Graph))
	{
		return TSharedPtr<SGraphEditor>();
	}

	// First, switch back to standard mode
	SetCurrentMode(FRigVMNewEditorApplicationModes::StandardRigVMEditorMode());

	// This will either reuse an existing tab or spawn a new one
	TSharedPtr<SDockTab> TabWithGraph = OpenDocument(Graph, FDocumentTracker::OpenNewDocument);
	if (TabWithGraph.IsValid())
	{

		// We know that the contents of the opened tabs will be a graph editor.
		TSharedRef<SGraphEditor> NewGraphEditor = StaticCastSharedRef<SGraphEditor>(TabWithGraph->GetContent());

		// Handover the keyboard focus to the new graph editor widget.
		if (bSetFocus)
		{
			NewGraphEditor->CaptureKeyboard();
		}

		return NewGraphEditor;
	}
	else
	{
		return TSharedPtr<SGraphEditor>();
	}
}

bool FRigVMNewEditor::FindOpenTabsContainingDocument(const UObject* DocumentID, TArray<TSharedPtr<SDockTab>>& Results)
{
	// TODO sara-s: Replicate what blueprint editor is doing
	return false;
}

TSharedPtr<SDockTab> FRigVMNewEditor::OpenDocument(const UObject* DocumentID, FDocumentTracker::EOpenDocumentCause Cause)
{
	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);
	return DocumentManager->OpenDocument(Payload, Cause);
}

void FRigVMNewEditor::CloseDocumentTab(const UObject* DocumentID)
{
	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);
	DocumentManager->CloseTab(Payload);
}

TSharedPtr<FApplicationMode> FRigVMNewEditor::CreateEditorMode()
{
	return MakeShareable(new FRigVMNewEditorMode(SharedThis(this)));
}

const FName FRigVMNewEditor::GetEditorAppName() const
{
	static const FLazyName AppName(TEXT("RigVMNewEditorApp"));
	return AppName;
}

void FRigVMNewEditor::OnGraphEditorFocused(const TSharedRef<SGraphEditor>& InGraphEditor)
{
	// Update the graph editor that is currently focused
	FocusedGraphEdPtr = InGraphEditor;

	// TODO sara-s
	//InGraphEditor->SetPinVisibility(PinVisibility);

	// Update the inspector as well, to show selection from the focused graph editor
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	FocusInspectorOnGraphSelection(SelectedNodes, /*bForceRefresh=*/ true);

	// During undo, garbage graphs can be temporarily brought into focus, ensure that before a refresh of the MyBlueprint window that the graph is owned by a Blueprint
	if ( FocusedGraphEdPtr.IsValid() && GraphExplorerWidget.IsValid() )
	{
		// The focused graph can be garbage as well
		TWeakObjectPtr< UEdGraph > FocusedGraphPtr = FocusedGraphEdPtr.Pin()->GetCurrentGraph();
		UEdGraph* FocusedGraph = FocusedGraphPtr.Get();

		if ( FocusedGraph != nullptr )
		{
			if ( FBlueprintEditorUtils::FindBlueprintForGraph(FocusedGraph) )
			{
				GraphExplorerWidget->Refresh();
			}
		}
	}

	// TODO sara-s
	// if (bHideUnrelatedNodes && SelectedNodes.Num() <= 0)
	// {
	// 	ResetAllNodesUnrelatedStates();
	// }

	// TODO sara-sc
	// If the bookmarks view is active, check whether or not we're restricting the view to the current graph. If we are, update the tree to reflect the focused graph context.
	// if (BookmarksWidget.IsValid()
	// 	&& GetDefault<UBlueprintEditorSettings>()->bShowBookmarksForCurrentDocumentOnlyInTab)
	// {
	// 	BookmarksWidget->RefreshBookmarksTree();
	// }

	FRigVMEditorBase::OnGraphEditorFocused(InGraphEditor);
}

void FRigVMNewEditor::OnGraphEditorBackgrounded(const TSharedRef<SGraphEditor>& InGraphEditor)
{
	// If the newly active document tab isn't a graph we want to make sure we clear the focused graph pointer.
	// Several other UI reads that, like the MyBlueprints view uses it to determine if it should show the "Local Variable" section.
	FocusedGraphEdPtr = nullptr;

	if ( GraphExplorerWidget.IsValid() == true )
	{
		GraphExplorerWidget->Refresh();
	}
}

FText FRigVMNewEditor::GetCompileStatusTooltip() const
{
	// Copied from FBlueprintEditorToolbar::GetStatusTooltip
	
	UBlueprint* BlueprintObj = GetBlueprintObj();
	EBlueprintStatus Status = BlueprintObj->Status;

	switch (Status)
	{
	case BS_Dirty:
		return LOCTEXT("Dirty_Status", "Dirty; needs to be recompiled");
	case BS_Error:
		return LOCTEXT("CompileError_Status", "There was an error during compilation, see the log for details");
	case BS_UpToDate:
		return LOCTEXT("GoodToGo_Status", "Good to go");
	case BS_UpToDateWithWarnings:
		return LOCTEXT("GoodToGoWarning_Status", "There was a warning during compilation, see the log for details");
	default:
		return LOCTEXT("Recompile_Status", "Unknown status; should recompile");
	}
}

FSlateIcon FRigVMNewEditor::GetCompileStatusImage() const
{
	// Copied from FBlueprintEditorToolbar::GetStatusImage
	
	UBlueprint* BlueprintObj = GetBlueprintObj();
	EBlueprintStatus Status = BlueprintObj->Status;

	// For macro types, always show as up-to-date, since we don't compile them
	if (BlueprintObj->BlueprintType == BPTYPE_MacroLibrary)
	{
		Status = BS_UpToDate;
	}

	
	static const FName CompileStatusBackground("Blueprint.CompileStatus.Background");
	static const FName CompileStatusUnknown("Blueprint.CompileStatus.Overlay.Unknown");
	static const FName CompileStatusError("Blueprint.CompileStatus.Overlay.Error");
	static const FName CompileStatusGood("Blueprint.CompileStatus.Overlay.Good");
	static const FName CompileStatusWarning("Blueprint.CompileStatus.Overlay.Warning");

	switch (Status)
	{
	case BS_Error:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusError);
	case BS_UpToDate:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusGood);
	case BS_UpToDateWithWarnings:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusWarning);
	default:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusUnknown);
	}
}

void FRigVMNewEditor::MakeSaveOnCompileSubMenu(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.BeginSection(TEXT("Section"));
	const FRigVMEditorCommands& Commands = FRigVMEditorCommands::Get();
	InMenuBuilder.AddMenuEntry(Commands.SaveOnCompile_Never);
	InMenuBuilder.AddMenuEntry(Commands.SaveOnCompile_SuccessOnly);
	InMenuBuilder.AddMenuEntry(Commands.SaveOnCompile_Always);
}

TSharedRef<SWidget> FRigVMNewEditor::GenerateCompileOptionsMenu()
{
	FMenuBuilder MenuBuilder(true, GetToolkitCommands());
	MenuBuilder.BeginSection(TEXT("Section"));
	const FRigVMEditorCommands& Commands = FRigVMEditorCommands::Get();

	// @TODO: disable the menu and change up the tooltip when all sub items are disabled
	MenuBuilder.AddSubMenu(
		LOCTEXT("SaveOnCompileSubMenu", "Save on Compile"),
		LOCTEXT("SaveOnCompileSubMenu_ToolTip", "Determines how the Blueprint is saved whenever you compile it."),
		FNewMenuDelegate::CreateSP(this, &FRigVMNewEditor::MakeSaveOnCompileSubMenu)
	);

	MenuBuilder.AddMenuEntry(Commands.JumpToErrorNode);
	MenuBuilder.AddMenuEntry(Commands.AutoCompileGraph);
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FRigVMNewEditor::AddCompileWidget(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddToolBarButton(FRigVMEditorCommands::Get().Compile,
		NAME_None, TAttribute<FText>(),
		TAttribute<FText>::CreateLambda([this]()
		{
			return GetCompileStatusTooltip();
		}),
		TAttribute<FSlateIcon>::CreateLambda([this]()
		{
			return GetCompileStatusImage();
		}),
		"CompileBlueprint");

	ToolbarBuilder.AddComboButton(
			FUIAction(),
				FOnGetContent::CreateSP(this, &FRigVMNewEditor::GenerateCompileOptionsMenu),
				TAttribute<FText>(),
		LOCTEXT("BlueprintCompileOptions_ToolbarTooltip", "Options to customize how Blueprints compile"),
		TAttribute<FSlateIcon>(), true);
}

void FRigVMNewEditor::AddSettingsAndDefaultWidget(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddToolBarButton(FRigVMEditorCommands::Get().EditGlobalOptions);
	ToolbarBuilder.AddToolBarButton(FRigVMEditorCommands::Get().EditClassDefaults);
}

void FRigVMNewEditor::AddSelectedDebugObjectWidget(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddWidget(SNew(SRigVMEditorSelectedDebugObjectWidget, SharedThis(this)));
}

void FRigVMNewEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	//@TODO: Can't we do this sooner?
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FRigVMNewEditor::Tick(float DeltaTime)
{
	// TODO sara-s: Replicate what blueprint editor is doing
	FRigVMEditorBase::Tick(DeltaTime);
}

void FRigVMNewEditor::GetPinTypeSelectorFilters(TArray<TSharedPtr<IPinTypeSelectorFilter>>& OutFilters) const
{
	// TODO sara-s
	// OutFilters.Add(ImportedPinTypeSelectorFilter);
	// OutFilters.Add(PermissionsPinTypeSelectorFilter);
}

void FRigVMNewEditor::OnAddNewVariable()
{
	const FScopedTransaction Transaction( LOCTEXT("AddVariable", "Add Variable") );

	FName VarName = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("NewVar"));

	bool bSuccess = GraphExplorerWidget.IsValid() && FBlueprintEditorUtils::AddMemberVariable(GetBlueprintObj(), VarName, GraphExplorerWidget->GetLastPinTypeUsed());

	if(!bSuccess)
	{
		LogSimpleMessage( LOCTEXT("AddVariable_Error", "Adding new variable failed.") );
	}
	else
	{
		RenameNewlyAddedAction(VarName);
	}
}

void FRigVMNewEditor::SummonSearchUI(bool bSetFindWithinBlueprint, FString NewSearchTerms, bool bSelectFirstResult)
{
	TSharedPtr<SRigVMFindReferences> FindResultsToUse = nullptr;

	FindResultsToUse = FindResults;
	TabManager->TryInvokeTab(FRigVMFindReferencesTabSummoner::TabID());
	
	if (FindResultsToUse.IsValid())
	{
		FindResultsToUse->FocusForUse(bSetFindWithinBlueprint, NewSearchTerms, bSelectFirstResult);
	}
}

void FRigVMNewEditor::ZoomToSelection_Clicked()
{
	if (SGraphEditor* GraphEd = FocusedGraphEdPtr.Pin().Get())
	{
		GraphEd->ZoomToFit(/*bOnlySelection=*/ true);
	}
}

void FRigVMNewEditor::RestoreEditedObjectState()
{
	check(IsEditingSingleBlueprint());	

	UBlueprint* Blueprint = GetBlueprintObj();
	if (Blueprint->LastEditedDocuments.Num() == 0)
	{
		if (FBlueprintEditorUtils::SupportsConstructionScript(Blueprint))
		{
			Blueprint->LastEditedDocuments.Add(FBlueprintEditorUtils::FindUserConstructionScript(Blueprint));
		}

		if (Blueprint->SupportsEventGraphs())
		{
			Blueprint->LastEditedDocuments.Add(FBlueprintEditorUtils::FindEventGraph(Blueprint));
		}
	}

	TSet<FSoftObjectPath> PathsToRemove;
	for (int32 i = 0; i < Blueprint->LastEditedDocuments.Num(); i++)
	{
		if (UObject* Obj = Blueprint->LastEditedDocuments[i].EditedObjectPath.ResolveObject())
		{
			if(UEdGraph* Graph = Cast<UEdGraph>(Obj))
			{
				if (FBlueprintEditorUtils::IsEventGraph(Graph) && !Blueprint->SupportsEventGraphs())
				{
					continue;
				}

				struct LocalStruct
				{
					static TSharedPtr<SDockTab> OpenGraphTree(FRigVMNewEditor* InBlueprintEditor, UEdGraph* InGraph)
					{
						FDocumentTracker::EOpenDocumentCause OpenCause = FDocumentTracker::QuickNavigateCurrentDocument;

						for (UObject* OuterObject = InGraph->GetOuter(); OuterObject; OuterObject = OuterObject->GetOuter())
						{
							if (OuterObject->IsA<UBlueprint>())
							{
								// reached up to the blueprint for the graph, we are done climbing the tree
								OpenCause = FDocumentTracker::RestorePreviousDocument;
								break;
							}
							else if(UEdGraph* OuterGraph = Cast<UEdGraph>(OuterObject))
							{
								// Found another graph, open it up
								OpenGraphTree(InBlueprintEditor, OuterGraph);
								break;
							}
						}

						return InBlueprintEditor->OpenDocument(InGraph, OpenCause);
					}
				};
				TSharedPtr<SDockTab> TabWithGraph = LocalStruct::OpenGraphTree(this, Graph);
				if (TabWithGraph.IsValid())
				{
					TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(TabWithGraph->GetContent());
					GraphEditor->SetViewLocation(Blueprint->LastEditedDocuments[i].SavedViewOffset, Blueprint->LastEditedDocuments[i].SavedZoomAmount);
				}
			}
			else
			{
				TSharedPtr<SDockTab> TabWithGraph = OpenDocument(Obj, FDocumentTracker::RestorePreviousDocument);
			}
		}
		else
		{
			PathsToRemove.Add(Blueprint->LastEditedDocuments[i].EditedObjectPath);
		}
	}

	// Older assets may have neglected to clean up this array when referenced objects were deleted, so
	// we'll check for that now. This is done to ensure we don't store invalid object paths indefinitely.
	if (PathsToRemove.Num() > 0)
	{
		Blueprint->LastEditedDocuments.RemoveAll([&PathsToRemove](const FEditedDocumentInfo& Entry)
		{
			return PathsToRemove.Contains(Entry.EditedObjectPath);
		});
	}
}

void FRigVMNewEditor::SetupViewForBlueprintEditingMode()
{
	// Make sure the defaults tab is pointing to the defaults
	StartEditingDefaults(/*bAutoFocus=*/ true);

	// Make sure the inspector is always on top
	//@TODO: This is necessary right now because of a bug in restoring layouts not remembering which tab is on top (to get it right initially), but do we want this behavior always?
	TryInvokingDetailsTab();
}

void FRigVMNewEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	// TODO sara-s: Replicate what blueprint editor is doing
	FWorkflowCentricApplication::InitToolMenuContext(MenuContext);
	FRigVMEditorBase::InitToolMenuContextImpl(MenuContext);
}

bool FRigVMNewEditor::TransactionObjectAffectsBlueprint(UObject* InTransactedObject)
{
	// TODO sara-s: Replicate what blueprint editor is doing
	return FRigVMEditorBase::TransactionObjectAffectsBlueprintImpl(InTransactedObject);
}

FEdGraphPinType FRigVMNewEditor::GetLastPinTypeUsed()
{
	if (GraphExplorerWidget.IsValid())
	{
		return GraphExplorerWidget->GetLastPinTypeUsed();
	}
	return FEdGraphPinType();
}

void FRigVMNewEditor::LogSimpleMessage(const FText& MessageText)
{
	FNotificationInfo Info( MessageText );
	Info.ExpireDuration = 3.0f;
	Info.bUseLargeFont = false;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_Fail );
	}
}

void FRigVMNewEditor::RenameNewlyAddedAction(FName InActionName)
{
	// TODO sara-s: Replicate what blueprint editor is doing
}

FGraphPanelSelectionSet FRigVMNewEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}
	return CurrentSelection;
}

void FRigVMNewEditor::SetUISelectionState(FName SelectionOwner)
{
	if ( SelectionOwner != CurrentUISelection )
	{
		ClearSelectionStateFor(CurrentUISelection);
	
		CurrentUISelection = SelectionOwner;
	}
}

void FRigVMNewEditor::AnalyticsTrackNodeEvent(UBlueprint* Blueprint, UEdGraphNode* GraphNode, bool bNodeDelete) const
{
	// TODO sara-s
	// if(Blueprint && GraphNode && FEngineAnalytics::IsAvailable())
	// {
	// 	// we'd like to see if this was happening in normal blueprint editor or persona 
	// 	//const FString EditorName = Cast<UAnimBlueprint>(Blueprint) != nullptr ? TEXT("Persona") : TEXT("BlueprintEditor");
	// 	const FString EditorName = TEXT("RigVMNewEditor");
	//
	// 	// Build Node Details
	// 	const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
	// 	FString ProjectID = ProjectSettings.ProjectID.ToString();
	// 	TArray<FAnalyticsEventAttribute> NodeAttributes;
	// 	NodeAttributes.Add(FAnalyticsEventAttribute(TEXT("ProjectId"), ProjectID));
	// 	NodeAttributes.Add(FAnalyticsEventAttribute(TEXT("BlueprintId"), Blueprint->GetBlueprintGuid().ToString()));
	// 	TArray<TKeyValuePair<FString, FString>> Attributes;
	//
	// 	if (UK2Node* K2Node = Cast<UK2Node>(GraphNode))
	// 	{
	// 		K2Node->GetNodeAttributes(Attributes);
	// 	}
	// 	else if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(GraphNode))
	// 	{
	// 		Attributes.Add(TKeyValuePair<FString, FString>(TEXT("Type"), TEXT("Comment")));
	// 		Attributes.Add(TKeyValuePair<FString, FString>(TEXT("Class"), CommentNode->GetClass()->GetName()));
	// 		Attributes.Add(TKeyValuePair<FString, FString>(TEXT("Name"), CommentNode->GetName()));
	// 	}
	// 	if (Attributes.Num() > 0)
	// 	{
	// 		// Build Node Attributes
	// 		for (const TKeyValuePair<FString, FString>& Attribute : Attributes)
	// 		{
	// 			NodeAttributes.Add(FAnalyticsEventAttribute(Attribute.Key, Attribute.Value));
	// 		}
	// 		// Send Analytics event 
	// 		FString EventType = bNodeDelete ?	FString::Printf(TEXT("Editor.Usage.%s.NodeDeleted"), *EditorName) :
	// 											FString::Printf(TEXT("Editor.Usage.%s.NodeCreated"), *EditorName);
	// 		FEngineAnalytics::GetProvider().RecordEvent(EventType, NodeAttributes);
	// 	}
	// }
}

void FRigVMNewEditor::JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename)
{
	if (FRigVMEditorBase::JumpToHyperlinkImpl(ObjectReference, bRequestRename))
	{
		return;
	}
	
	SetCurrentMode(FRigVMNewEditorApplicationModes::StandardRigVMEditorMode());

	if (const UEdGraphNode* Node = Cast<const UEdGraphNode>(ObjectReference))
	{
		if (bRequestRename)
		{
			IsNodeTitleVisible(Node, bRequestRename);
		}
		else
		{
			JumpToNode(Node, false);
		}
	}
	else if (const UEdGraph* Graph = Cast<const UEdGraph>(ObjectReference))
	{
		// Navigating into things should re-use the current tab when it makes sense
		FDocumentTracker::EOpenDocumentCause OpenMode = FDocumentTracker::OpenNewDocument;
		if ((Graph->GetSchema()->GetGraphType(Graph) == GT_Ubergraph) || Cast<UK2Node>(Graph->GetOuter()) || Cast<UEdGraph>(Graph->GetOuter()))
		{
			// Ubergraphs directly reuse the current graph
			OpenMode = FDocumentTracker::NavigatingCurrentDocument;
		}
		else
		{
			// Walk up the outer chain to see if any tabs have a parent of this document open for edit, and if so
			// we should reuse that one and drill in deeper instead
			for (UObject* WalkPtr = const_cast<UEdGraph*>(Graph); WalkPtr != nullptr; WalkPtr = WalkPtr->GetOuter())
			{
				TArray< TSharedPtr<SDockTab> > TabResults;
				if (FindOpenTabsContainingDocument(WalkPtr, /*out*/ TabResults))
				{
					// See if the parent was active
					bool bIsActive = false;
					for (TSharedPtr<SDockTab> Tab : TabResults)
					{
						if (Tab->IsActive())
						{
							bIsActive = true;
							break;
						}
					}

					if (bIsActive)
					{
						OpenMode = FDocumentTracker::NavigatingCurrentDocument;
						break;
					}
				}
			}
		}

		// Force it to open in a new document if shift is pressed
		const bool bIsShiftPressed = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
		if (bIsShiftPressed)
		{
			OpenMode = FDocumentTracker::ForceOpenNewDocument;
		}

		// Open the document
		OpenDocument(Graph, OpenMode);
	}
	else if (const AActor* ReferencedActor = Cast<const AActor>(ObjectReference))
	{
		// Check if the world is active in the editor. It's possible to open level BPs without formally opening
		// the levels through Find-in-Blueprints
		bool bInOpenWorld = false;
		const TIndirectArray<FWorldContext>& WorldContextList = GEditor->GetWorldContexts();
		const UWorld* ReferencedActorOwningWorld = ReferencedActor->GetWorld();
		for (const FWorldContext& WorldContext : WorldContextList)
		{
			if (WorldContext.World() == ReferencedActorOwningWorld)
			{
				bInOpenWorld = true;
				break;
			}
		}

		// Clear the selection even if we couldn't find it, so the existing selection doesn't get mistaken for the desired to be selected actor
		GEditor->SelectNone(false, false);

		if (bInOpenWorld)
		{
			// Select the in-level actor
			GEditor->SelectActor(const_cast<AActor*>(ReferencedActor), true, true, true);

			// Point the camera at it
			GUnrealEd->Exec(ReferencedActor->GetWorld(), TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));
		}
	}
	else if(const UFunction* Function = Cast<const UFunction>(ObjectReference))
	{
		UBlueprint* BP = GetBlueprintObj();
		if(BP)
		{
			if (UEdGraph* FunctionGraph = FBlueprintEditorUtils::FindScopeGraph(BP, Function))
			{
				OpenDocument(FunctionGraph, FDocumentTracker::OpenNewDocument);
			}
		}
	}
	else if(const UBlueprintGeneratedClass* Class = Cast<const UBlueprintGeneratedClass>(ObjectReference))
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Class->ClassGeneratedBy);
	}
	// else if (const UTimelineTemplate* Timeline = Cast<const UTimelineTemplate>(ObjectReference))
	// {
	// 	OpenDocument(Timeline, FDocumentTracker::OpenNewDocument);
	// }
	else if ((ObjectReference != nullptr) && ObjectReference->IsAsset())
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(const_cast<UObject*>(ObjectReference));
	}
	else
	{
		UE_LOG(LogBlueprint, Warning, TEXT("Unknown type of hyperlinked object (%s), cannot focus it"), *GetNameSafe(ObjectReference));
	}

	//@TODO: Hacky way to ensure a message is seen when hitting an exception and doing intraframe debugging
	const FText ExceptionMessage = FKismetDebugUtilities::GetAndClearLastExceptionMessage();
	if (!ExceptionMessage.IsEmpty())
	{
		LogSimpleMessage( ExceptionMessage );
	}
}

void FRigVMNewEditor::PostUndo(bool bSuccess)
{
	FEditorUndoClient::PostUndo(bSuccess);
	FRigVMEditorBase::PostUndoImpl(bSuccess);
}

void FRigVMNewEditor::PostRedo(bool bSuccess)
{
	FEditorUndoClient::PostRedo(bSuccess);
	FRigVMEditorBase::PostRedoImpl(bSuccess);
}

UEdGraphPin* FRigVMNewEditor::GetCurrentlySelectedPin() const
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		return FocusedGraphEd->GetGraphPinForMenu();
	}

	return nullptr;
}

void FRigVMNewEditor::SetSaveOnCompileSetting(ESaveOnCompile NewSetting)
{
	// Copied from BlueprintEditorImpl::SetSaveOnCompileSetting
	
	UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
	Settings->SaveOnCompile = NewSetting;
	Settings->SaveConfig();
}

bool FRigVMNewEditor::IsSaveOnCompileEnabled() const
{
	// Copied from BlueprintEditorImpl::IsSaveOnCompileEnabled
	
	UBlueprint* Blueprint = GetBlueprintObj();
	bool const bIsLevelScript = (Cast<ULevelScriptBlueprint>(Blueprint) != nullptr);

	return !bIsLevelScript;
}

bool FRigVMNewEditor::IsSaveOnCompileOptionSet(TWeakPtr<FRigVMNewEditor> Editor, ESaveOnCompile Option)
{
	// Copied from BlueprintEditorImpl::IsSaveOnCompileOptionSet
	
	const UBlueprintEditorSettings* Settings = GetDefault<UBlueprintEditorSettings>();

	ESaveOnCompile CurrentSetting = Settings->SaveOnCompile;
	if (!Editor.IsValid() || !Editor.Pin()->IsSaveOnCompileEnabled())
	{
		// if save-on-compile is disabled for the blueprint, then we want to 
		// show "Never" as being selected
		// 
		// @TODO: a tooltip explaining why would be nice too
		CurrentSetting = SoC_Never;
	}

	return (CurrentSetting == Option);
}

void FRigVMNewEditor::ToggleJumpToErrorNodeSetting()
{
	// Copied from BlueprintEditorImpl::ToggleJumpToErrorNodeSetting
	
	UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
	Settings->bJumpToNodeErrors = !Settings->bJumpToNodeErrors;
	Settings->SaveConfig();
}

bool FRigVMNewEditor::IsJumpToErrorNodeOptionSet()
{
	// Copied from BlueprintEditorImpl::IsJumpToErrorNodeOptionSet
	
	const UBlueprintEditorSettings* Settings = GetDefault<UBlueprintEditorSettings>();
	return Settings->bJumpToNodeErrors;
}

UEdGraphNode* FRigVMNewEditor::FindNodeWithError(UBlueprint* Blueprint, EMessageSeverity::Type Severity/* = EMessageSeverity::Error*/)
{
	// Copied from BlueprintEditorImpl::FindNodeWithError
	
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	UEdGraphNode* ChoiceNode = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->bHasCompilerMessage && !Node->ErrorMsg.IsEmpty() && (Node->ErrorType <= Severity))
			{
				if ((ChoiceNode == nullptr) || (ChoiceNode->ErrorType > Node->ErrorType))
				{
					ChoiceNode = Node;
					if (ChoiceNode->ErrorType == 0)
					{
						break;
					}
				}
			}
		}
	}
	return ChoiceNode;
}

UEdGraphNode* FRigVMNewEditor::FindNodeWithError(FCompilerResultsLog const& ErrorLog, EMessageSeverity::Type Severity/* = EMessageSeverity::Error*/)
{
	// Copied from BlueprintEditorImpl::FindNodeWithError
	
	UEdGraphNode* ChoiceNode = nullptr;
	for (TWeakObjectPtr<UEdGraphNode> NodePtr : ErrorLog.AnnotatedNodes)
	{
		UEdGraphNode* Node = NodePtr.Get();
		if ((Node != nullptr) && (Node->ErrorType <= Severity))
		{
			if ((ChoiceNode == nullptr) || (Node->ErrorType < ChoiceNode->ErrorType))
			{
				ChoiceNode = Node;
				if (ChoiceNode->ErrorType == 0)
				{
					break;
				}
			}
		}
	}

	return ChoiceNode;
}

void FRigVMNewEditor::CreateDefaultCommands()
{
	if (GetBlueprintObj())
	{
		// TODO sara-s: Replicate what blueprint editor is doing
		//FBlueprintEditor::CreateDefaultCommands();

		GetToolkitCommands()->MapAction(
			FRigVMEditorCommands::Get().Compile,
			FExecuteAction::CreateSP(this, &FRigVMEditorBase::Compile),
			FCanExecuteAction::CreateSP(this, &FRigVMEditorBase::IsCompilingEnabled));

		TWeakPtr<FRigVMNewEditor> WeakThisPtr = SharedThis(this);
		ToolkitCommands->MapAction(
			FRigVMEditorCommands::Get().SaveOnCompile_Never,
			FExecuteAction::CreateSP(this, &FRigVMNewEditor::SetSaveOnCompileSetting, (ESaveOnCompile)SoC_Never),
			FCanExecuteAction::CreateSP(this, &FRigVMNewEditor::IsSaveOnCompileEnabled),
			FIsActionChecked::CreateSP(this, &FRigVMNewEditor::IsSaveOnCompileOptionSet, WeakThisPtr, (ESaveOnCompile)SoC_Never)
		);
		ToolkitCommands->MapAction(
			FRigVMEditorCommands::Get().SaveOnCompile_SuccessOnly,
			FExecuteAction::CreateSP(this, &FRigVMNewEditor::SetSaveOnCompileSetting, (ESaveOnCompile)SoC_SuccessOnly),
			FCanExecuteAction::CreateSP(this, &FRigVMNewEditor::IsSaveOnCompileEnabled),
			FIsActionChecked::CreateSP(this, &FRigVMNewEditor::IsSaveOnCompileOptionSet, WeakThisPtr, (ESaveOnCompile)SoC_SuccessOnly)
		);
		ToolkitCommands->MapAction(
			FRigVMEditorCommands::Get().SaveOnCompile_Always,
			FExecuteAction::CreateSP(this, &FRigVMNewEditor::SetSaveOnCompileSetting, (ESaveOnCompile)SoC_Always),
			FCanExecuteAction::CreateSP(this, &FRigVMNewEditor::IsSaveOnCompileEnabled),
			FIsActionChecked::CreateSP(this, &FRigVMNewEditor::IsSaveOnCompileOptionSet, WeakThisPtr, (ESaveOnCompile)SoC_Always)
		);

		ToolkitCommands->MapAction(
			FRigVMEditorCommands::Get().JumpToErrorNode,
			FExecuteAction::CreateSP(this, &FRigVMNewEditor::ToggleJumpToErrorNodeSetting),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FRigVMNewEditor::IsJumpToErrorNodeOptionSet)
		);

		ToolkitCommands->MapAction(
			FRigVMEditorCommands::Get().EditGlobalOptions,
			FExecuteAction::CreateSP(this, &FRigVMNewEditor::EditGlobalOptions_Clicked),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FRigVMNewEditor::IsDetailsPanelEditingGlobalOptions));

		ToolkitCommands->MapAction(
			FRigVMEditorCommands::Get().EditClassDefaults,
			FExecuteAction::CreateSP(this, &FRigVMNewEditor::EditClassDefaults_Clicked),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FRigVMNewEditor::IsDetailsPanelEditingClassDefaults));
	}

	FRigVMEditorBase::CreateDefaultCommandsImpl();
}

TSharedRef<SGraphEditor> FRigVMNewEditor::CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph)
{
	check((InGraph != nullptr) && IsEditingSingleBlueprint());

	// No need to regenerate the commands.
	if(!GraphEditorCommands.IsValid())
	{
		GraphEditorCommands = MakeShareable( new FUICommandList );
		{
			
			// Alignment Commands
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesTop,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::OnAlignTop )
				);
			
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesMiddle,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::OnAlignMiddle )
				);
			
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesBottom,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::OnAlignBottom )
				);
			
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesLeft,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::OnAlignLeft )
				);
			
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesCenter,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::OnAlignCenter )
				);
			
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesRight,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::OnAlignRight )
				);
			
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().StraightenConnections,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::OnStraightenConnections )
				);
			
			// Distribution Commands
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().DistributeNodesHorizontally,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::OnDistributeNodesH )
				);
			
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().DistributeNodesVertically,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::OnDistributeNodesV )
				);

			// Editing commands
			GraphEditorCommands->MapAction( FGenericCommands::Get().SelectAll,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::SelectAllNodes ),
				FCanExecuteAction::CreateSP( this, &FRigVMNewEditor::CanSelectAllNodes )
				);
			
			GraphEditorCommands->MapAction( FGenericCommands::Get().Delete,
				FExecuteAction::CreateSP( this, &FRigVMEditorBase::DeleteSelectedNodes ),
				FCanExecuteAction::CreateSP( this, &FRigVMEditorBase::CanDeleteNodes )
				);
			
			GraphEditorCommands->MapAction( FGenericCommands::Get().Copy,
				FExecuteAction::CreateSP( this, &FRigVMEditorBase::CopySelectedNodes ),
				FCanExecuteAction::CreateSP( this, &FRigVMEditorBase::CanCopyNodes )
				);
			
			GraphEditorCommands->MapAction( FGenericCommands::Get().Cut,
				FExecuteAction::CreateSP( this, &FRigVMEditorBase::CutSelectedNodes ),
				FCanExecuteAction::CreateSP( this, &FRigVMEditorBase::CanCutNodes )
				);
			
			GraphEditorCommands->MapAction( FGenericCommands::Get().Paste,
				FExecuteAction::CreateSP( this, &FRigVMEditorBase::PasteNodes ),
				FCanExecuteAction::CreateSP( this, &FRigVMEditorBase::CanPasteNodes )
				);
			
			GraphEditorCommands->MapAction( FGenericCommands::Get().Duplicate,
				FExecuteAction::CreateSP( this, &FRigVMEditorBase::DuplicateNodes ),
				FCanExecuteAction::CreateSP( this, &FRigVMEditorBase::CanDuplicateNodes )
				);
			
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().StartWatchingPin,
				FExecuteAction::CreateSP( this, &FRigVMEditorBase::OnStartWatchingPin ),
				FCanExecuteAction::CreateSP( this, &FRigVMEditorBase::CanStartWatchingPin )
				);
			
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().StopWatchingPin,
				FExecuteAction::CreateSP( this, &FRigVMEditorBase::OnStopWatchingPin ),
				FCanExecuteAction::CreateSP( this, &FRigVMEditorBase::CanStopWatchingPin )
				);
			
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().CreateComment,
				FExecuteAction::CreateSP( this, &FRigVMNewEditor::OnCreateComment )
				);

			OnCreateGraphEditorCommands(GraphEditorCommands);
		}
	}

	// Create the title bar widget
	TSharedPtr<SWidget> TitleBarWidget = SNew(SRigVMGraphTitleBar)
		.EdGraphObj(InGraph)
		.Editor(SharedThis(this))
		.HistoryNavigationWidget(InTabInfo->CreateHistoryNavigationWidget());

	SGraphEditor::FGraphEditorEvents InEvents;
	SetupGraphEditorEvents(InGraph, InEvents);

	// Append play world commands
	GraphEditorCommands->Append( FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef() );

	TSharedRef<SGraphEditor> Editor = SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		//.IsEditable(this, &FBlueprintEditor::IsEditable, InGraph)
		//.DisplayAsReadOnly(this, &FBlueprintEditor::IsGraphReadOnly, InGraph)
		.TitleBar(TitleBarWidget)
		//.Appearance(this, &FBlueprintEditor::GetGraphAppearance, InGraph)
		.GraphToEdit(InGraph)
		.GraphEvents(InEvents)
		//.OnNavigateHistoryBack(FSimpleDelegate::CreateSP(this, &FBlueprintEditor::NavigateTab, FDocumentTracker::NavigateBackwards))
		//.OnNavigateHistoryForward(FSimpleDelegate::CreateSP(this, &FBlueprintEditor::NavigateTab, FDocumentTracker::NavigateForwards))
		.AssetEditorToolkit(this->GetHostingApp());
		//@TODO: Crashes in command list code during the callback .OnGraphModuleReloaded(FEdGraphEvent::CreateSP(this, &FBlueprintEditor::ChangeOpenGraphInDocumentEditorWidget, WeakParent))
		;
		
	//OnSetPinVisibility.AddSP(&Editor.Get(), &SGraphEditor::SetPinVisibility);

	FVector2f ViewOffset = FVector2f::ZeroVector;
	float ZoomAmount = INDEX_NONE;

	TSharedPtr<SDockTab> ActiveTab = DocumentManager->GetActiveTab();
	if(ActiveTab.IsValid())
	{
		// Check if the graph is already opened in the current tab, if it is we want to start at the same position to stop the graph from jumping around oddly
		TSharedPtr<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(ActiveTab->GetContent());

		if(GraphEditor.IsValid() && GraphEditor->GetCurrentGraph() == InGraph)
		{
			GraphEditor->GetViewLocation(ViewOffset, ZoomAmount);
		}
	}

	Editor->SetViewLocation(ViewOffset, ZoomAmount);
	
	Editor->GetGraphPanel()->SetZoomLevelsContainer<FRigVMEditorZoomLevelsContainer>();
	return Editor;
}

void FRigVMNewEditor::CompileImpl()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UBlueprint* BlueprintObj = GetBlueprintObj();
	if (BlueprintObj)
	{
		FMessageLog BlueprintLog("BlueprintLog");

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("BlueprintName"), FText::FromString(BlueprintObj->GetName()));
		BlueprintLog.NewPage(FText::Format(LOCTEXT("CompilationPageLabel", "Compile {BlueprintName}"), Arguments));

		FCompilerResultsLog LogResults;
		LogResults.SetSourcePath(BlueprintObj->GetPathName());
		LogResults.BeginEvent(TEXT("Compile"));
		
		// TODO: sara-s remove once blueprint backend is replaced
		{
			EBlueprintCompileOptions CompileOptions = EBlueprintCompileOptions::None;

			// If compilation is enabled during PIE/simulation, references to the CDO might be held by a script variable.
			// Thus, we set the flag to direct the compiler to allow those references to be replaced during reinstancing.
			if (GEditor->PlayWorld != nullptr)
			{
				CompileOptions |= EBlueprintCompileOptions::IncludeCDOInReferenceReplacement;
			}
		
			FKismetEditorUtilities::CompileBlueprint(BlueprintObj, CompileOptions, &LogResults);
		}

		LogResults.EndEvent();

		// CachedNumWarnings = LogResults.NumWarnings;
		// CachedNumErrors = LogResults.NumErrors;

		const bool bForceMessageDisplay = (LogResults.NumWarnings > 0 || LogResults.NumErrors > 0) && !BlueprintObj->bIsRegeneratingOnLoad;
		DumpMessagesToCompilerLog(LogResults.Messages, bForceMessageDisplay);

		if (BlueprintObj->UpgradeNotesLog.IsValid())
		{
			CompilerResultsListing->AddMessages(BlueprintObj->UpgradeNotesLog->Messages);
		}

		// send record when player clicks compile and send the result
		// this will make sure how the users activity is
		//AnalyticsTrackCompileEvent(BlueprintObj, LogResults.NumErrors, LogResults.NumWarnings);

		RefreshInspector();
	}
}

bool FRigVMNewEditor::IsGraphInCurrentBlueprint(const UEdGraph* InGraph) const
{
	bool bEditable = true;

	UBlueprint* EditingBP = GetBlueprintObj();
	if(EditingBP)
	{
		TArray<UEdGraph*> Graphs;
		EditingBP->GetAllGraphs(Graphs);
		bEditable &= Graphs.Contains(InGraph);
	}

	return bEditable;
}

bool FRigVMNewEditor::IsEditableImpl(UEdGraph* InGraph) const
{
	return InEditingMode() && !FBlueprintEditorUtils::IsGraphReadOnly(InGraph);
}

UEdGraph* FRigVMNewEditor::GetFocusedGraph() const
{
	if (FocusedGraphEdPtr.IsValid())
	{
		if (UEdGraph* Graph = FocusedGraphEdPtr.Pin()->GetCurrentGraph())
		{
			if (IsValid(Graph))
			{
				return Graph;
			}
		}
	}
	return nullptr;
}

void FRigVMNewEditor::JumpToNode(const UEdGraphNode* Node, bool bRequestRename)
{
	TSharedPtr<SGraphEditor> GraphEditor;
	if(bRequestRename)
	{
		// If we are renaming, the graph will be open already, just grab the tab and it's content and jump to the node.
		TSharedPtr<SDockTab> ActiveTab = DocumentManager->GetActiveTab();
		check(ActiveTab.IsValid());
		GraphEditor = StaticCastSharedRef<SGraphEditor>(ActiveTab->GetContent());
	}
	else
	{
		// Open a graph editor and jump to the node
		GraphEditor = OpenGraphAndBringToFront(Node->GetGraph());
	}

	if (GraphEditor.IsValid())
	{
		GraphEditor->JumpToNode(Node, bRequestRename);
	}
}

void FRigVMNewEditor::JumpToPin(const UEdGraphPin* Pin)
{
	if (!Pin->IsPendingKill())
	{
		// Open a graph editor and jump to the pin
		TSharedPtr<SGraphEditor> GraphEditor = OpenGraphAndBringToFront(Pin->GetOwningNode()->GetGraph());
		if (GraphEditor.IsValid())
		{
			GraphEditor->JumpToPin(Pin);
		}
	}
}

void FRigVMNewEditor::OnBlueprintChangedInnerImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled)
{
	if (InBlueprint)
	{
		// Notify that the blueprint has been changed (update Content browser, etc)
		InBlueprint->PostEditChange();

		// Call PostEditChange() on any Actors that are based on this Blueprint 
		FBlueprintEditorUtils::PostEditChangeBlueprintActors(InBlueprint);

		// Refresh the graphs
		ERefreshRigVMEditorReason::Type Reason = bIsJustBeingCompiled ? ERefreshRigVMEditorReason::BlueprintCompiled : ERefreshRigVMEditorReason::UnknownReason;
		ForceEditorRefresh(Reason);

		// In case objects were deleted, which should close the tab
		if (GetCurrentMode() == FRigVMNewEditorApplicationModes::StandardRigVMEditorMode())
		{
			// Copied from FBlueprintEditor::SaveEditedObjectState()
            {
            	check(IsEditingSingleBlueprint());
            
            	// Clear currently edited documents
            	GetBlueprintObj()->LastEditedDocuments.Empty();
            
            	// Ask all open documents to save their state, which will update LastEditedDocuments
            	DocumentManager->SaveAllState();
            }
		}
	}
}

void FRigVMNewEditor::RefreshEditorsImpl(ERefreshRigVMEditorReason::Type Reason)
{
	// TODO sara-s: Replicate what blueprint editor is doing
	bool bForceFocusOnSelectedNodes = false;

	// if (CurrentUISelection == SelectionState_MyBlueprint)
	// {
	// 	// Handled below, here to avoid tripping the ensure
	// }
	// else if (CurrentUISelection == SelectionState_Components)
	// {
	// 	if(SubobjectEditor.IsValid())
	// 	{
	// 		SubobjectEditor->RefreshSelectionDetails();
	// 	}
	// }
	// else if (CurrentUISelection == SelectionState_Graph)
	// {
	// 	bForceFocusOnSelectedNodes = true;
	// }
	// else if (CurrentUISelection == SelectionState_ClassSettings)
	// {
	// 	// No need for a refresh, the Blueprint object didn't change
	// }
	// else if (CurrentUISelection == SelectionState_ClassDefaults)
	// {
	// 	StartEditingDefaults(/*bAutoFocus=*/ false, true);
	// }

	// Remove any tabs are that are pending kill or otherwise invalid UObject pointers.
	DocumentManager->CleanInvalidTabs();

	//@TODO: Should determine when we need to do the invalid/refresh business and if the graph node selection change
	// under non-compiles is necessary (except when the selection mode is appropriate, as already detected above)
	if (Reason != ERefreshRigVMEditorReason::BlueprintCompiled)
	{
		DocumentManager->RefreshAllTabs();

		bForceFocusOnSelectedNodes = true;
	}

	if (bForceFocusOnSelectedNodes)
	{
		FocusInspectorOnGraphSelection(GetSelectedNodes(), /*bForceRefresh=*/ true);
	}

	// if (ReplaceReferencesWidget.IsValid())
	// {
	// 	ReplaceReferencesWidget->Refresh();
	// }

	if (GraphExplorerWidget.IsValid())
	{
		GraphExplorerWidget->Refresh();
	}

	// if(SubobjectEditor.IsValid())
	// {
	// 	SubobjectEditor->RefreshComponentTypesList();
	// 	SubobjectEditor->UpdateTree();
	//
	// 	// Note: Don't pass 'true' here because we don't want the preview actor to be reconstructed until after Blueprint modification is complete.
	// 	UpdateSubobjectPreview();
	// }
	//
	// if (BookmarksWidget.IsValid())
	// {
	// 	BookmarksWidget->RefreshBookmarksTree();
	// }

	// Note: There is an optimization inside of ShowDetailsForSingleObject() that skips the refresh if the object being selected is the same as the previous object.
	// The SKismetInspector class is shared between both Defaults mode and Components mode, but in Defaults mode the object selected is always going to be the CDO. Given
	// that the selection does not really change, we force it to refresh and skip the optimization. Otherwise, some things may not work correctly in Defaults mode. For
	// example, transform details are customized and the rotation value is cached at customization time; if we don't force refresh here, then after an undo of a previous
	// rotation edit, transform details won't be re-customized and thus the cached rotation value will be stale, resulting in an invalid rotation value on the next edit.
	//@TODO: Probably not always necessary
	//RefreshStandAloneDefaultsEditor();

	// Update associated controls like the function editor
	//BroadcastRefresh();
}

void FRigVMNewEditor::SetupGraphEditorEventsImpl(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents)
{
	// TODO sara-s: Replicate what blueprint editor is doing
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP( this, &FRigVMNewEditor::OnSelectedNodesChanged );
	// InEvents.OnVerifyTextCommit = FOnNodeVerifyTextCommit::CreateSP(this, &FRigVMNewEditor::OnNodeVerifyTitleCommit);
	// InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FRigVMNewEditor::OnNodeTitleCommitted);
	// InEvents.OnSpawnNodeByShortcut = SGraphEditor::FOnSpawnNodeByShortcut::CreateSP(this, &FRigVMNewEditor::OnSpawnGraphNodeByShortcut, InGraph);
	// InEvents.OnNodeSpawnedByKeymap = SGraphEditor::FOnNodeSpawnedByKeymap::CreateSP(this, &FRigVMNewEditor::OnNodeSpawnedByKeymap );
	// InEvents.OnDisallowedPinConnection = SGraphEditor::FOnDisallowedPinConnection::CreateSP(this, &FRigVMNewEditor::OnDisallowedPinConnection);
	// InEvents.OnDoubleClicked = SGraphEditor::FOnDoubleClicked::CreateSP(this, &FRigVMNewEditor::NavigateToParentGraphByDoubleClick);
	//

	// Custom menu for K2 schemas
	if(InGraph->Schema != nullptr && InGraph->Schema->IsChildOf(UEdGraphSchema_K2::StaticClass()))
	{
		InEvents.OnCreateActionMenuAtLocation = SGraphEditor::FOnCreateActionMenuAtLocation::CreateSP(this, &FRigVMNewEditor::OnCreateGraphActionMenu);
	}
}

FActionMenuContent FRigVMNewEditor::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2f& InNodePosition,
	const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	// TODO sara-s: Replicate what blueprint editor is doing
	HasOpenActionMenu = InGraph;
	// if (!BlueprintEditorImpl::GraphHasUserPlacedNodes(InGraph))
	// {
	// 	InstructionsFadeCountdown = BlueprintEditorImpl::InstructionFadeDuration;
	// }

	TSharedRef<SRigVMActionMenu> ActionMenu = 
		SNew(SRigVMActionMenu, SharedThis(this))
		.GraphObj(InGraph)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins);

	return FActionMenuContent( ActionMenu, ActionMenu->GetFilterTextBox() );
}

void FRigVMNewEditor::AddReferencedObjectsImpl(FReferenceCollector& Collector)
{
	
}

bool FRigVMNewEditor::IsSectionVisible(RigVMNodeSectionID::Type InSectionID) const
{
	return FRigVMEditorBase::IsSectionVisibleImpl(InSectionID);
}

bool FRigVMNewEditor::NewDocument_IsVisibleForType(ECreatedDocumentType GraphType) const
{
	return FRigVMEditorBase::NewDocument_IsVisibleForTypeImpl(GraphType);
}

FGraphAppearanceInfo FRigVMNewEditor::GetGraphAppearanceImpl(UEdGraph* InGraph) const
{
	// Create the appearance info
	FGraphAppearanceInfo AppearanceInfo;

	UBlueprint* Blueprint = (InGraph != nullptr) ? FBlueprintEditorUtils::FindBlueprintForGraph(InGraph) : GetBlueprintObj();
	if (Blueprint != nullptr)
	{
		if (FBlueprintEditorUtils::IsEditorUtilityBlueprint(Blueprint))
		{
			AppearanceInfo.CornerText = LOCTEXT("EditorUtilityAppearanceCornerText", "EDITOR UTILITY");
		}
		else
		{
			switch (Blueprint->BlueprintType)
			{
			case BPTYPE_LevelScript:
				AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_LevelScript", "LEVEL BLUEPRINT");
				break;
			case BPTYPE_MacroLibrary:
				AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Macro", "MACRO");
				break;
			case BPTYPE_Interface:
				AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Interface", "INTERFACE");
				break;
			default:
				AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Blueprint", "BLUEPRINT");
				break;
			}
		}
	}

	UEdGraph const* EditingGraph = GetFocusedGraph();
	if (InGraph && RigVMNewEditorImpl::GraphHasDefaultNode(InGraph))
	{
		AppearanceInfo.InstructionText = LOCTEXT("AppearanceInstructionText_DefaultGraph", "Drag Off Pins to Create/Connect New Nodes.");
	}
	else // if the graph is empty...
		{
		AppearanceInfo.InstructionText = LOCTEXT("AppearanceInstructionText_EmptyGraph", "Right-Click to Create New Nodes.");
		}
	auto InstructionOpacityDelegate = TAttribute<float>::FGetter::CreateSP(this, &FRigVMNewEditor::GetInstructionTextOpacity, InGraph);
	AppearanceInfo.InstructionFade.Bind(InstructionOpacityDelegate);

	// Copy from FBlueprintEditor::GetPIEStatus
	{
		UBlueprint* CurrentBlueprint = GetBlueprintObj();
		UWorld *DebugWorld = nullptr;
		ENetMode NetMode = NM_Standalone;
		if (CurrentBlueprint)
		{
			DebugWorld = CurrentBlueprint->GetWorldBeingDebugged();
			if (DebugWorld)
			{
				NetMode = DebugWorld->GetNetMode();
			}
			else
			{
				UObject* ObjOuter = CurrentBlueprint->GetObjectBeingDebugged();
				while(DebugWorld == nullptr && ObjOuter != nullptr)
				{
					ObjOuter = ObjOuter->GetOuter();
					DebugWorld = Cast<UWorld>(ObjOuter);
				}

				if (DebugWorld)
				{
					// Redirect through streaming levels to find the owning world; this ensures that we always use the appropriate NetMode for the context string below.
					if (DebugWorld->PersistentLevel != nullptr && DebugWorld->PersistentLevel->OwningWorld != nullptr)
					{
						DebugWorld = DebugWorld->PersistentLevel->OwningWorld;
					}

					NetMode = DebugWorld->GetNetMode();
				}
			}
		}

		if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
		{
			AppearanceInfo.PIENotifyText = LOCTEXT("PIEStatusServerSimulating", "SERVER - SIMULATING");
		}
		else if (NetMode == NM_Client)
		{
			FWorldContext* PIEContext = GEngine->GetWorldContextFromWorld(DebugWorld);
			if (PIEContext && PIEContext->PIEInstance > 1)
			{
				AppearanceInfo.PIENotifyText = FText::Format(LOCTEXT("PIEStatusClientSimulatingFormat", "CLIENT {0} - SIMULATING"), FText::AsNumber(PIEContext->PIEInstance - 1));
			}
		
			AppearanceInfo.PIENotifyText = LOCTEXT("PIEStatusClientSimulating", "CLIENT - SIMULATING");
		}

		AppearanceInfo.PIENotifyText = LOCTEXT("PIEStatusSimulating", "SIMULATING");
	}

	return AppearanceInfo;
}

void FRigVMNewEditor::NotifyPreChangeImpl(FProperty* PropertyAboutToChange)
{
	// this only delivers message to the "FOCUSED" one, not every one
	// internally it will only deliver the message to the selected node, not all nodes
	FString PropertyName = PropertyAboutToChange->GetName();
	if (FocusedGraphEdPtr.IsValid())
	{
		FocusedGraphEdPtr.Pin()->NotifyPrePropertyChange(PropertyName);
	}
}

void FRigVMNewEditor::NotifyPostChangeImpl(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	FString PropertyName = PropertyThatChanged->GetName();
	if (FocusedGraphEdPtr.IsValid())
	{
		FocusedGraphEdPtr.Pin()->NotifyPostPropertyChange(PropertyChangedEvent, PropertyName);
	}
	
	if (IsEditingSingleBlueprint())
	{
		UBlueprint* Blueprint = GetBlueprintObj();
		UPackage* BlueprintPackage = Blueprint->GetOutermost();

		// if any of the objects being edited are in our package, mark us as dirty
		bool bPropertyInBlueprint = false;
		for (int32 ObjectIndex = 0; ObjectIndex < PropertyChangedEvent.GetNumObjectsBeingEdited(); ++ObjectIndex)
		{
			const UObject* Object = PropertyChangedEvent.GetObjectBeingEdited(ObjectIndex);
			if (Object && Object->GetOutermost() == BlueprintPackage)
			{
				bPropertyInBlueprint = true;
				break;
			}
		}

		if (bPropertyInBlueprint)
		{
			// Note: if change type is "interactive," hold off on applying the change (e.g. this will occur if the user is scrubbing a spinbox value; we don't want to apply the change until the mouse is released, for performance reasons)
			if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint, PropertyChangedEvent);

				// Call PostEditChange() on any Actors that might be based on this Blueprint
				FBlueprintEditorUtils::PostEditChangeBlueprintActors(Blueprint);
			}

			// TODO sara-s
			// Force updates to occur immediately during interactive mode (otherwise the preview won't refresh because it won't be ticking)
			//UpdateSubobjectPreview(PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive);
		}
	}
}

FName FRigVMNewEditor::GetSelectedVariableName()
{
	FName VariableName;
	if (GraphExplorerWidget.IsValid())
	{
		return GraphExplorerWidget->GetSelectedVariableName();
	}
	return VariableName;
}

bool FRigVMNewEditor::IsNodeTitleVisible(const UEdGraphNode* Node, bool bRequestRename)
{
	TSharedPtr<SGraphEditor> GraphEditor;
	if(bRequestRename)
	{
		// If we are renaming, the graph will be open already, just grab the tab and it's content and jump to the node.
		TSharedPtr<SDockTab> ActiveTab = DocumentManager->GetActiveTab();
		check(ActiveTab.IsValid());
		GraphEditor = StaticCastSharedRef<SGraphEditor>(ActiveTab->GetContent());
	}
	else
	{
		// Open a graph editor and jump to the node
		GraphEditor = OpenGraphAndBringToFront(Node->GetGraph());
	}

	bool bVisible = false;
	if (GraphEditor.IsValid())
	{
		bVisible = GraphEditor->IsNodeTitleVisible(Node, bRequestRename);
	}
	return bVisible;
}

void FRigVMNewEditor::EditClassDefaults_Clicked()
{
	StartEditingDefaults(true, true);
}

void FRigVMNewEditor::EditGlobalOptions_Clicked()
{
	SetUISelectionState(FRigVMNewEditor::SelectionState_ClassSettings());

	if (bWasOpenedInDefaultsMode)
	{
		RefreshStandAloneDefaultsEditor();
	}
	else
	{
		UBlueprint* Blueprint = GetBlueprintObj();
		if (Blueprint != nullptr)
		{
			// Show details for the Blueprint instance we're editing
			if (Inspector.IsValid())
			{
				Inspector->ShowDetailsForSingleObject(Blueprint);
			}

			TryInvokingDetailsTab();
		}
	}
}

bool FRigVMNewEditor::IsDetailsPanelEditingGlobalOptions() const
{
	return CurrentUISelection == FRigVMNewEditor::SelectionState_ClassSettings();
}

bool FRigVMNewEditor::IsDetailsPanelEditingClassDefaults() const
{
	return CurrentUISelection == FRigVMNewEditor::SelectionState_ClassDefaults();
}

void FRigVMNewEditor::TryInvokingDetailsTab(bool bFlash)
{
	if ( TabManager->HasTabSpawner(FRigVMDetailsInspectorTabSummoner::TabID()) )
	{
		TSharedPtr<SDockTab> BlueprintTab = FGlobalTabmanager::Get()->GetMajorTabForTabManager(TabManager.ToSharedRef());

		// We don't want to force this tab into existence when the blueprint editor isn't in the foreground and actively
		// being interacted with.  So we make sure the window it's in is focused and the tab is in the foreground.
		if ( BlueprintTab.IsValid() && BlueprintTab->IsForeground() )
		{
			if ( !Inspector.IsValid() || !Inspector->GetOwnerTab().IsValid() || Inspector->GetOwnerTab()->GetDockArea().IsValid() )
			{
				// Show the details panel if it doesn't exist.
				TabManager->TryInvokeTab(FRigVMDetailsInspectorTabSummoner::TabID());

				if ( bFlash && Inspector.IsValid())
				{
					TSharedPtr<SDockTab> OwnerTab = Inspector->GetOwnerTab();
					if ( OwnerTab.IsValid() )
					{
						OwnerTab->FlashTab();
					}
				}
			}
		}
	}
}

void FRigVMNewEditor::RefreshInspector()
{
	if (Inspector.IsValid())
	{
	    Inspector->GetPropertyView()->ForceRefresh();
	}
}

void FRigVMNewEditor::RefreshStandAloneDefaultsEditor()
{
	// Update the details panel
	SRigVMDetailsInspector::FShowDetailsOptions Options(FText::GetEmpty(), true);

	TArray<UObject*> DefaultObjects;
	for ( int32 i = 0; i < GetEditingObjects().Num(); ++i )
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(GetEditingObjects()[i]))
		{
			if (CurrentUISelection == FRigVMNewEditor::SelectionState_ClassSettings())
			{
				DefaultObjects.Add(Blueprint);
			}
			else if (Blueprint->GeneratedClass)
			{
				DefaultObjects.Add(Blueprint->GeneratedClass->GetDefaultObject());
			}
		}
	}

	if ( DefaultObjects.Num() && DefaultEditor.IsValid() )
	{
		DefaultEditor->ShowDetailsForObjects(DefaultObjects);
	}
}

void FRigVMNewEditor::StartEditingDefaults(bool bAutoFocus, bool bForceRefresh)
{
	SetUISelectionState(FRigVMNewEditor::SelectionState_ClassDefaults());

	if (IsEditingSingleBlueprint())
	{
		if (GetBlueprintObj()->GeneratedClass != nullptr)
		{
			// TODO sara-s
			// if (SubobjectEditor.IsValid() && GetBlueprintObj()->GeneratedClass->IsChildOf<AActor>())
			// {
			// 	SubobjectEditor->SelectRoot();
			// }
			// else
			{
				UObject* DefaultObject = GetBlueprintObj()->GeneratedClass->GetDefaultObject();

				// Update the details panel
				FString Title;
				DefaultObject->GetName(Title);

				SRigVMDetailsInspector::FShowDetailsOptions Options(FText::FromString(Title), bForceRefresh);

				if (Inspector.IsValid())
				{
					Inspector->ShowDetailsForSingleObject(DefaultObject, Options);
				}

				if ( bAutoFocus )
				{
					TryInvokingDetailsTab();
				}
			}
		}
	}
	
	RefreshStandAloneDefaultsEditor();
}

float FRigVMNewEditor::GetInstructionTextOpacity(UEdGraph* InGraph) const
{
	bool bGraphReadOnly = true;
	if (InGraph)
	{
		bGraphReadOnly = !InGraph->bEditable;

		if (!bGraphReadOnly)
		{
			const UBlueprint* BlueprintForGraph = FBlueprintEditorUtils::FindBlueprintForGraph(InGraph);
			bool const bIsInterface = ((BlueprintForGraph != nullptr) && (BlueprintForGraph->BlueprintType == BPTYPE_Interface));
			bool const bIsDelegate  = FBlueprintEditorUtils::IsDelegateSignatureGraph(InGraph);
			bool const bIsMathExpression = FBlueprintEditorUtils::IsMathExpressionGraph(InGraph);

			bGraphReadOnly = bIsInterface || bIsDelegate || bIsMathExpression;
		}
	}
	
	URigVMEditorSettings const* Settings = GetDefault<URigVMEditorSettings>();
	if ((InGraph == nullptr) || !IsEditable(InGraph) || bGraphReadOnly)
	{
		return 0.0f;
	}
	else if ((InstructionsFadeCountdown > 0.0f) || (HasOpenActionMenu == InGraph))
	{
		return InstructionsFadeCountdown / RigVMNewEditorImpl::InstructionFadeDuration;
	}
	else if (RigVMNewEditorImpl::GraphHasUserPlacedNodes(InGraph))
	{
		return 0.0f;
	}
	return 1.0f;
}

void FRigVMNewEditor::ClearSelectionStateFor(FName SelectionOwner)
{
	if ( SelectionOwner == SelectionState_Graph() )
	{
		TArray< TSharedPtr<SDockTab> > GraphEditorTabs;
		DocumentManager->FindAllTabsForFactory(GraphEditorTabFactoryPtr, /*out*/ GraphEditorTabs);

		for (TSharedPtr<SDockTab>& GraphEditorTab : GraphEditorTabs)
		{
			TSharedRef<SGraphEditor> Editor = StaticCastSharedRef<SGraphEditor>((GraphEditorTab)->GetContent());

			Editor->ClearSelectionSet();
		}
	}
	else if ( SelectionOwner == SelectionState_GraphExplorer() )
	{
		if ( GraphExplorerWidget.IsValid() )
		{
			GraphExplorerWidget->ClearSelection();
		}
	}
}

void FRigVMNewEditor::OnLogTokenClicked(const TSharedRef<IMessageToken>& MessageToken)
{
	if (MessageToken->GetType() == EMessageToken::EdGraph)
	{
		const TSharedRef<FEdGraphToken> EdGraphToken = StaticCastSharedRef<FEdGraphToken>(MessageToken);
		const UEdGraphPin* PinBeingReferenced = EdGraphToken->GetPin();
		const UObject* ObjectBeingReferenced = EdGraphToken->GetGraphObject();
		if (PinBeingReferenced)
		{
			JumpToPin(PinBeingReferenced);
		}
		else if(ObjectBeingReferenced)
		{
			JumpToHyperlink(ObjectBeingReferenced, false);
		}
	}
}

void FRigVMNewEditor::DumpMessagesToCompilerLog(const TArray<TSharedRef<class FTokenizedMessage>>& Messages, bool bForceMessageDisplay)
{
	CompilerResultsListing->ClearMessages();

	// Note we dont mirror to the output log here as the compiler already does that
	CompilerResultsListing->AddMessages(Messages, false);
	
	if (bForceMessageDisplay)
	{
		TabManager->TryInvokeTab(FRigVMCompilerResultsTabSummoner::TabID());
	}
}

void FRigVMNewEditor::CreateDefaultTabContents(const TArray<UBlueprint*> InBlueprints)
{
	UBlueprint* InBlueprint = InBlueprints.Num() == 1 ? InBlueprints[0] : nullptr;

	// Cache off whether or not this is an interface, since it is used to govern multiple widget's behavior
	const bool bIsInterface = (InBlueprint && InBlueprint->BlueprintType == BPTYPE_Interface);
	const bool bIsMacro = (InBlueprint && InBlueprint->BlueprintType == BPTYPE_MacroLibrary);

	if (InBlueprint)
	{
		// TODO sara-s
		// this->BookmarksWidget =
		// 	SNew(SBlueprintBookmarks)
		// 		.EditorContext(SharedThis(this));
	}

	if (IsEditingSingleBlueprint())
	{
		//this->ReplaceReferencesWidget = SNew(SReplaceNodeReferences, SharedThis(this)); // TODO sara-s
	}
	
	CompilerResultsListing = FCompilerResultsLog::GetBlueprintMessageLog(InBlueprint);
	CompilerResultsListing->OnMessageTokenClicked().AddSP(this, &FRigVMNewEditor::OnLogTokenClicked); 

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	CompilerResults = MessageLogModule.CreateLogListingWidget( CompilerResultsListing.ToSharedRef() );
	FindResults = SNew(SRigVMFindReferences, SharedThis(this));
	
	this->Inspector = 
		SNew(SRigVMDetailsInspector)
		.Editor(SharedThis(this))
		. HideNameArea(true)
		. ViewIdentifier(FName("BlueprintInspector"))
		. OnFinishedChangingProperties( FOnFinishedChangingProperties::FDelegate::CreateSP(this, &FRigVMNewEditor::OnFinishedChangingProperties) ); 
	;

	if ( InBlueprints.Num() > 0 )
	{
		// Don't show the object name in defaults mode.
		const bool bHideNameArea = bWasOpenedInDefaultsMode;

		this->DefaultEditor = 
			SNew(SRigVMDetailsInspector)
			.Editor(SharedThis(this))
			. ViewIdentifier(FName("BlueprintDefaults"))
			. IsEnabled(!bIsInterface)
			//. ShowPublicViewControl(this, &FBlueprintEditor::ShouldShowPublicViewControl) // TODO sara-s
			. ShowTitleArea(false)
			. HideNameArea(bHideNameArea)
			. OnFinishedChangingProperties( FOnFinishedChangingProperties::FDelegate::CreateSP( this, &FRigVMNewEditor::OnFinishedChangingProperties ) ); 
		;
	}

	if (InBlueprint && 
		InBlueprint->ParentClass &&
		InBlueprint->ParentClass->IsChildOf(AActor::StaticClass()) && 
		InBlueprint->SimpleConstructionScript )
	{
		//CreateSubobjectEditors(); // TODO sara-s
	}
}

const FSlateBrush* FRigVMNewEditor::GetGlyphForGraph(const UEdGraph* Graph, bool bInLargeIcon)
{
	const FSlateBrush* ReturnValue = FAppStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.Function_24x") : TEXT("GraphEditor.Function_16x") );

	check(Graph != nullptr);
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema != nullptr)
	{
		const EGraphType GraphType = Schema->GetGraphType(Graph);
		switch (GraphType)
		{
		default:
		case GT_StateMachine:
		case GT_Macro:
		case GT_Animation:
			check(false);
			break;
		case GT_Function:
			{
				bool bIsSubGraph = false;
				{
					if( Graph->GetOuter() )
					{
						//Check whether the outer is a composite node
						if( Graph->GetOuter()->IsA( UK2Node_Composite::StaticClass() ) )
						{
							bIsSubGraph = true;
						}
					}
				}
				
				//Check for subgraph
				if( bIsSubGraph )
				{
					ReturnValue = FAppStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.SubGraph_24x") : TEXT("GraphEditor.SubGraph_16x") );
				}
				else
				{
					ReturnValue = FAppStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.Function_24x") : TEXT("GraphEditor.Function_16x") );
				}
			}
			break;
		case GT_Ubergraph:
			{
				ReturnValue = FAppStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.EventGraph_24x") : TEXT("GraphEditor.EventGraph_16x") );
			}
			break;
		}
	}

	return ReturnValue;
}

void FRigVMNewEditor::OnSelectedNodesChanged(const FGraphPanelSelectionSet& NewSelection)
{
	OnSelectedNodesChangedImpl(NewSelection);
}

void FRigVMNewEditor::OnAlignTop()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->OnAlignTop();
	}
}

void FRigVMNewEditor::OnAlignMiddle()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->OnAlignMiddle();
	}
}

void FRigVMNewEditor::OnAlignBottom()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->OnAlignBottom();
	}
}

void FRigVMNewEditor::OnAlignLeft()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->OnAlignLeft();
	}
}

void FRigVMNewEditor::OnAlignCenter()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->OnAlignCenter();
	}
}

void FRigVMNewEditor::OnAlignRight()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->OnAlignRight();
	}
}

void FRigVMNewEditor::OnStraightenConnections()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->OnStraightenConnections();
	}
}

void FRigVMNewEditor::OnDistributeNodesH()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->OnDistributeNodesH();
	}
}

void FRigVMNewEditor::OnDistributeNodesV()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->OnDistributeNodesV();
	}
}

void FRigVMNewEditor::SelectAllNodes()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->SelectAllNodes();
	}
}

bool FRigVMNewEditor::CanSelectAllNodes() const
{
	return true;
}


#undef LOCTEXT_NAMESPACE
