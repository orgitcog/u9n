// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditorSettings.h"
#include "RigVMEditor.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

class SRigVMFindReferences;
class SRigVMDetailsInspector;

struct RIGVMEDITOR_API FRigVMNewEditorTabs
{
	// Tab identifiers
	static const FName CompilerResultsID();
};

class RIGVMEDITOR_API FRigVMNewEditor : public FWorkflowCentricApplication, public FRigVMEditorBase, public FGCObject, public FNotifyHook, public FTickableEditorObject, public FEditorUndoClient, public FNoncopyable
{
public:
	FRigVMNewEditor();
	virtual void OnClose() override;

	virtual TSharedRef<IRigVMEditor> SharedRef() override { return StaticCastSharedRef<IRigVMEditor>(SharedThis(this)); }
	virtual TSharedRef<const IRigVMEditor> SharedRef() const override { return StaticCastSharedRef<const IRigVMEditor>(SharedThis(this)); }

	virtual TSharedPtr<FAssetEditorToolkit> GetHostingApp() override { return AsShared(); }
	virtual const TSharedPtr<FAssetEditorToolkit> GetHostingApp() const override { return ConstCastSharedRef<FAssetEditorToolkit>(AsShared()); }
protected:
	
	virtual void InitAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const FName AppIdentifier, const TSharedRef<FTabManager::FLayout>& StandaloneDefaultLayout, const bool bCreateDefaultStandaloneMenu, const bool bCreateDefaultToolbar, const TArray<UObject*>& ObjectsToEdit, const bool bInIsToolbarFocusable, const bool bInUseSmallToolbarIcons, const TOptional<EAssetOpenMethod>& InOpenMethod) override;
	virtual void CreateEditorToolbar() override {}
	virtual void CommonInitialization(const TArray<UBlueprint*>& InitBlueprints, bool bShouldOpenInDefaultsMode) override;
	void OnBlueprintChanged(UBlueprint* InBlueprint);
	void SaveEditedObjectState();
	virtual TSharedPtr<FDocumentTracker> GetDocumentManager() const override { return DocumentManager; }
	virtual void AddApplicationMode(FName ModeName, TSharedRef<FApplicationMode> Mode) override { FWorkflowCentricApplication::AddApplicationMode(ModeName, Mode); }
	virtual void RegenerateMenusAndToolbars() override { FWorkflowCentricApplication::RegenerateMenusAndToolbars(); }
	virtual void SetCurrentMode(FName NewMode) override;
	virtual FEditorModeTools& GetToolkitEditorModeManager() const override { return FWorkflowCentricApplication::GetEditorModeManager(); }
	virtual void PostLayoutBlueprintEditorInitialization() override;
	virtual TSharedPtr<SGraphEditor> OpenGraphAndBringToFront(UEdGraph* Graph, bool bSetFocus = true) override;
	virtual bool FindOpenTabsContainingDocument(const UObject* DocumentID, /*inout*/ TArray< TSharedPtr<SDockTab> >& Results) override;
	virtual TSharedPtr<SDockTab> OpenDocument(const UObject* DocumentID, FDocumentTracker::EOpenDocumentCause Cause) override;
	virtual void CloseDocumentTab(const UObject* DocumentID) override;
	virtual TSharedPtr<FTabManager> GetTabManager() override { return FWorkflowCentricApplication::GetTabManager(); }
public:
	virtual TSharedPtr<SRigVMDetailsInspector> GetRigVMInspector() const override { return Inspector.IsValid() ? Inspector.ToSharedRef().ToSharedPtr() : nullptr; }
	virtual void SetInspector(TSharedPtr<SRigVMDetailsInspector> InWidget) { Inspector = InWidget; };
	
	TSharedRef<SWidget> GetCompilerResults() const { return CompilerResults.ToSharedRef(); }
	TSharedRef<SRigVMFindReferences> GetFindResults() const { return FindResults.ToSharedRef(); }
	virtual void JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename) override;
protected:
	virtual TSharedPtr<IMessageLogListing> GetCompilerResultsListing() override { return CompilerResultsListing; }
	virtual TSharedPtr<FApplicationMode> CreateEditorMode() override;
	virtual const FName GetEditorAppName() const override;
	virtual const TArray< UObject* >& GetEditingBlueprints() const override { return FWorkflowCentricApplication::GetEditingObjects(); }
	virtual const TSharedRef<IToolkitHost> GetToolkitHost() const override { return FWorkflowCentricApplication::GetToolkitHost(); }
	virtual bool IsHosted() const override { return FWorkflowCentricApplication::IsHosted(); }
	virtual void BringToolkitToFrontImpl() override { FWorkflowCentricApplication::BringToolkitToFront(); }
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;
	virtual TSharedRef<FUICommandList> GetToolkitCommands() override { return ToolkitCommands; }
	virtual TWeakPtr<SGraphEditor> GetFocusedGraphEditor() override { return FocusedGraphEdPtr; }
	virtual TWeakPtr<FDocumentTabFactory> GetGraphEditorTabFactory() const override { return GraphEditorTabFactoryPtr; }
	virtual bool TransactionObjectAffectsBlueprint(UObject* InTransactedObject) override;
	virtual FEdGraphPinType GetLastPinTypeUsed() override;
	virtual void LogSimpleMessage(const FText& MessageText) override;
	virtual void RenameNewlyAddedAction(FName InActionName) override;
	virtual FGraphPanelSelectionSet GetSelectedNodes() const override;
	virtual void SetUISelectionState(FName SelectionOwner) override;
	virtual void AnalyticsTrackNodeEvent(UBlueprint* Blueprint, UEdGraphNode* GraphNode, bool bNodeDelete) const override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	virtual UEdGraphPin* GetCurrentlySelectedPin() const override;
	virtual void CreateDefaultCommands() override;
	virtual TSharedRef<SGraphEditor> CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph) override;
	virtual void CompileImpl() override;
	virtual void SaveAsset_Execute_Impl() override { FWorkflowCentricApplication::SaveAsset_Execute(); }
	virtual void SaveAssetAs_Execute_Impl() override { FWorkflowCentricApplication::SaveAssetAs_Execute(); }
	virtual bool IsGraphInCurrentBlueprint(const UEdGraph* InGraph) const override;
	virtual bool IsEditableImpl(UEdGraph* InGraph) const override;
	virtual UEdGraph* GetFocusedGraph() const override;
	virtual void JumpToNode(const UEdGraphNode* Node, bool bRequestRename) override;
	virtual void JumpToPin(const UEdGraphPin* Pin) override;
	virtual void AddToolbarExtender(TSharedPtr<FExtender> Extender) override { FWorkflowCentricApplication::AddToolbarExtender(Extender); }
	virtual void RemoveToolbarExtender(TSharedPtr<FExtender> Extender) override { FWorkflowCentricApplication::RemoveToolbarExtender(Extender); };
	virtual void AddMenuExtender(TSharedPtr<FExtender> Extender) override { FWorkflowCentricApplication::AddMenuExtender(Extender); }
	virtual void RemoveMenuExtender(TSharedPtr<FExtender> Extender) override { FWorkflowCentricApplication::RemoveMenuExtender(Extender); }
	virtual void OnBlueprintChangedInnerImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled) override;
	virtual void RefreshEditorsImpl(ERefreshRigVMEditorReason::Type Reason) override;
	virtual void SetupGraphEditorEventsImpl(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents) override;
	virtual FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2f& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed) override;
	virtual void AddReferencedObjectsImpl(FReferenceCollector& Collector) override;
	virtual bool IsSectionVisible(RigVMNodeSectionID::Type InSectionID) const; 
	virtual bool NewDocument_IsVisibleForType(ECreatedDocumentType GraphType) const;
	virtual FGraphAppearanceInfo GetGraphAppearanceImpl(UEdGraph* InGraph) const override;
	virtual void NotifyPreChangeImpl(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChangeImpl(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	virtual FName GetSelectedVariableName() override;
	virtual bool IsNodeTitleVisible(const UEdGraphNode* Node, bool bRequestRename) override;
	virtual void EditClassDefaults_Clicked() override;
	virtual void EditGlobalOptions_Clicked() override;
	bool IsDetailsPanelEditingGlobalOptions() const;
	bool IsDetailsPanelEditingClassDefaults() const;
	virtual void TryInvokingDetailsTab(bool bFlash = true) override;
	virtual FName GetGraphExplorerWidgetID() override { return FRigVMEditorGraphExplorerTabSummoner::TabID(); }
	virtual void RefreshInspector() override;
	virtual void RefreshStandAloneDefaultsEditor() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetPinTypeSelectorFilters(TArray<TSharedPtr<IPinTypeSelectorFilter>>& OutFilters) const override;
	virtual void OnAddNewVariable() override;
	virtual void ZoomToSelection_Clicked() override;
public:
	virtual void SummonSearchUI(bool bSetFindWithinBlueprint, FString NewSearchTerms, bool bSelectFirstResult) override;
	virtual void RestoreEditedObjectState() override;
	virtual void SetupViewForBlueprintEditingMode() override;
	virtual void OnGraphEditorFocused(const TSharedRef<SGraphEditor>& InGraphEditor);
	virtual void OnGraphEditorBackgrounded(const TSharedRef<SGraphEditor>& InGraphEditor);
	virtual bool GetIsContextSensitive() override { return bIsActionMenuContextSensitive; }
	virtual void SetIsContextSensitive(const bool bIsContextSensitive) override { bIsActionMenuContextSensitive = bIsContextSensitive; }
	virtual void RegisterToolbarTab(const TSharedRef<FTabManager>& InTabManager) override { FAssetEditorToolkit::RegisterTabSpawners(InTabManager); }
	virtual const TArray<UObject*>* GetObjectsCurrentlyBeingEdited() const override { return FAssetEditorToolkit::GetObjectsCurrentlyBeingEdited(); }
	virtual void AddCompileWidget(FToolBarBuilder& ToolbarBuilder) override;
	virtual void AddSettingsAndDefaultWidget(FToolBarBuilder& ToolbarBuilder) override;
	virtual void AddSelectedDebugObjectWidget(FToolBarBuilder& ToolbarBuilder) override;
	virtual void AddAutoCompileWidget(FToolBarBuilder& ToolbarBuilder) override {}
	virtual void Compile() { FRigVMEditorBase::Compile(); }
	virtual void OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList) { FRigVMEditorBase::OnCreateGraphEditorCommands(GraphEditorCommandsList); }
	virtual bool ShouldOpenGraphByDefault() const { return FRigVMEditorBase::ShouldOpenGraphByDefault(); }
	virtual void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) { FRigVMEditorBase::OnFinishedChangingProperties(PropertyChangedEvent); }
	virtual void HandleSetObjectBeingDebugged(UObject* InObject) { FRigVMEditorBase::HandleSetObjectBeingDebugged(InObject); }
	virtual FReply OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2f& InPosition, UEdGraph* InGraph) { return FRigVMEditorBase::OnSpawnGraphNodeByShortcut(InChord, InPosition, InGraph); }
	virtual FPreviewScene* GetPreviewScene() override { return nullptr; }

	//~ Begin IToolkit Interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	//~ End IToolkit Interface


	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FRigVMEditorBase::GetWorldCentricTabColorScale(); }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { return FRigVMEditorBase::AddReferencedObjects(Collector); }
	virtual FString GetReferencerName() const override { return TEXT("FRigVMNewEditor"); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FRigVMNewEditor, STATGROUP_Tickables); }
	virtual void StartEditingDefaults(bool bAutoFocus = true, bool bForceRefresh = false);

	float GetInstructionTextOpacity(UEdGraph* InGraph) const;
	virtual void ClearSelectionStateFor(FName SelectionOwner);
protected:
	void OnLogTokenClicked(const TSharedRef<IMessageToken>& MessageToken);

	/** Dumps messages to the compiler log, with an option to force it to display/come to front */
	void DumpMessagesToCompilerLog(const TArray<TSharedRef<class FTokenizedMessage>>& Messages, bool bForceMessageDisplay);
public:
	void CreateDefaultTabContents(const TArray<UBlueprint*> InBlueprints);

	TSharedRef<SWidget> GenerateCompileOptionsMenu();
	void MakeSaveOnCompileSubMenu(FMenuBuilder& InMenu);
	void SetSaveOnCompileSetting(ESaveOnCompile NewSetting);
	bool IsSaveOnCompileEnabled() const;
	bool IsSaveOnCompileOptionSet(TWeakPtr<FRigVMNewEditor> Editor, ESaveOnCompile Option);
	void ToggleJumpToErrorNodeSetting();
	bool IsJumpToErrorNodeOptionSet();
	UEdGraphNode* FindNodeWithError(UBlueprint* Blueprint, EMessageSeverity::Type Severity);
	UEdGraphNode* FindNodeWithError(FCompilerResultsLog const& ErrorLog, EMessageSeverity::Type Severity);
	FText GetCompileStatusTooltip() const;
	FSlateIcon GetCompileStatusImage() const;

public:
	static const FSlateBrush* GetGlyphForGraph(const UEdGraph* Graph, bool bInLargeIcon);

	static const FName SelectionState_GraphExplorer();
	static const FName SelectionState_Graph();
	static const FName SelectionState_ClassSettings();
	static const FName SelectionState_ClassDefaults();

	virtual FNotifyHook* GetNotifyHook() override { return this; }

	void OnSelectedNodesChanged(const FGraphPanelSelectionSet& NewSelection);

	void OnAlignTop();
	void OnAlignMiddle();
	void OnAlignBottom();
	void OnAlignLeft();
	void OnAlignCenter();
	void OnAlignRight();
	void OnStraightenConnections();
	void OnDistributeNodesH();
	void OnDistributeNodesV();
	void SelectAllNodes();
	bool CanSelectAllNodes() const;

protected:
	
	TSharedPtr<FDocumentTracker> DocumentManager;

	/** Node inspector widget */
	TSharedPtr<class SRigVMDetailsInspector> Inspector;

	/** Currently focused graph editor */
	TWeakPtr<class SGraphEditor> FocusedGraphEdPtr;

	/** Factory that spawns graph editors; used to look up all tabs spawned by it. */
	TWeakPtr<FDocumentTabFactory> GraphEditorTabFactoryPtr;

	/** The current UI selection state of this editor */
	FName CurrentUISelection;

	/** Command list for the graph editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	/** Compiler results log, with the log listing that it reflects */
	TSharedPtr<class SWidget> CompilerResults;
	TSharedPtr<class IMessageLogListing> CompilerResultsListing;

	/** Find results log as well as the search filter */
	TSharedPtr<class SRigVMFindReferences> FindResults;

	/** When set, flags which graph has a action menu currently open (if null, no graphs do). */
	UEdGraph* HasOpenActionMenu;
	
	/** Used to nicely fade instruction text, when the context menu is opened. */
	float InstructionsFadeCountdown;

	/** defaults inspector widget */
	TSharedPtr<class SRigVMDetailsInspector> DefaultEditor;
	
	/** True if the editor was opened in defaults mode */
	bool bWasOpenedInDefaultsMode;

	/** Did we update the blueprint when it opened */
	bool bBlueprintModifiedOnOpen;

	/** Whether the graph action menu should be sensitive to the pins dragged off of */
	bool bIsActionMenuContextSensitive;
};


