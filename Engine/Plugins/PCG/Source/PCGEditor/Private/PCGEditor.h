// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "Graph/PCGStackContext.h"

#include "EditorUndoClient.h"
#include "GraphEditor.h"
#include "Toolkits/AssetEditorToolkit.h"

enum class ECheckBoxState : uint8;
namespace ETextCommit { enum Type : int; }

class FUICommandList;
class SGraphEditor;
class SGraphEditorActionMenu;
class SPCGEditorGraphAttributeListView;
class SPCGEditorGraphDebugObjectTree;
class SPCGEditorGraphDetailsView;
class SPCGEditorGraphDeterminismListView;
class SPCGEditorGraphFind;
class SPCGEditorGraphLogView;
class SPCGEditorGraphNodePalette;
class SPCGEditorGraphProfilingView;
class SPCGEditorNodeSource;
class SPCGEditorGraphUserParametersView;
class SPCGEditorViewport;
class UEdGraphNode;
class UPCGComponent;
class UPCGEditorGraph;
class UPCGEditorGraphNodeBase;
class UPCGGraph;
struct FPCGCompilerDiagnostics;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInspectedStackChanged, const FPCGStack&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInspectedNodeChanged, UPCGEditorGraphNodeBase*);

enum class EPCGEditorPanel
{
	Attributes1,
	Attributes2,
	Attributes3,
	Attributes4,
	DebugObjectTree,
	Determinism,
	Find,
	GraphEditor,
	Log,
	NodePalette,
	NodeSource,
	Profiling,
	PropertyDetails1,
	PropertyDetails2,
	PropertyDetails3,
	PropertyDetails4,
	UserParams,
	Viewport1,
	Viewport2,
	Viewport3,
	Viewport4
};

class FPCGEditor : public FAssetEditorToolkit, public FGCObject, public FSelfRegisteringEditorUndoClient
{
public:
	/** Edits the specified PCGGraph */
	void Initialize(const EToolkitMode::Type InMode, const TSharedPtr<class IToolkitHost>& InToolkitHost, UPCGGraph* InPCGGraph);

	/** Get the PCG editor graph being edited. */
	UPCGEditorGraph* GetPCGEditorGraph();

	/** Gets/Creates the PCG graph editor for a given PCG graph */
	static UPCGEditorGraph* GetPCGEditorGraph(UPCGGraph* InGraph);
	static UPCGEditorGraph* GetPCGEditorGraph(const UPCGNode* InNode);
	static UPCGEditorGraph* GetPCGEditorGraph(const UPCGSettings* InSettings);

	/** Get the PCG graph being edited. */
	const UPCGGraph* GetPCGGraph() { return PCGGraphBeingEdited; }

	/** Sets the execution stack that want to inspect. */
	void SetStackBeingInspected(const FPCGStack& FullStack);

	/** Sets the execution stack from another editor, which will set directly in the debug object tree view. */
	void SetStackBeingInspectedFromAnotherEditor(const FPCGStack& FullStack);

	/** Clear current inspection. */
	void ClearStackBeingInspected();

	/** Gets the PCG component we are debugging */
	UPCGComponent* GetPCGComponentBeingInspected() const { return const_cast<UPCGComponent*>(StackBeingInspected.GetRootComponent()); }
	
	/** Gets the PCG stack we are inspecting */
	const FPCGStack* GetStackBeingInspected() const;

	void SetSourceEditorTargetObject(UObject* InObject);

	/** Focus the graph view on a specific node */
	void JumpToNode(const UEdGraphNode* InNode);
	void JumpToNode(const UPCGNode* InNode);

	/** Get the TabID of the editor panel. */
	FName GetPanelID(EPCGEditorPanel Panel) const;
	/** Focuses the user on a specific panel and flashes the tab. */
	void BringFocusToPanel(EPCGEditorPanel Panel) const;
	/** Attempts to close the specific panel if it's open. */
	void CloseGraphPanel(EPCGEditorPanel Panel) const;
	/** Returns true if the selected tab is currently open. */
	bool IsPanelCurrentlyOpen(EPCGEditorPanel Panel) const;
	/** Returns true if the selected tab is currently open and focused. */
	bool IsPanelCurrentlyForeground(EPCGEditorPanel Panel) const;

	/** Helper to get to the subsystem. */
	static class UPCGSubsystem* GetSubsystem();

	// ~Begin IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	// ~End IToolkit interface

	// ~Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FPCGEditor");
	}
	// ~End FGCObject interface
	
	// ~Begin FEditorUndoClient interface
	virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// ~End FEditorUndoClient interface

	// ~Begin FAssetEditorToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual void OnClose() override;
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;
	// ~End FAssetEditorToolkit interface

	/**
	 * Handles spawning a graph node in the current graph using the passed in chord.
	 *
	 * @param	InChord		Chord that was just performed.
	 * @param	InPosition	Current cursor position.
	 * @param	InGraph		Graph that chord was performed in.
	 *
	 * @return	FReply	Whether chord was handled.
	 */
	UE_DEPRECATED(5.6, "Please use the version of the function accepting FVector2f.")
	FReply OnSpawnNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UPCGEditorGraph* InGraph);
	FReply OnSpawnNodeByShortcut(FInputChord InChord, const FVector2f& InPosition, UPCGEditorGraph* InGraph);

	FActionMenuContent OnCreateActionMenuContent(UEdGraph* InGraph, const FVector2f& Location, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnMenuClosed);

	FOnInspectedStackChanged OnInspectedStackChangedDelegate;

	/** Can determinism be tested on the current graph */
	bool CanRunDeterminismGraphTest() const;
	/** Run the determinism test on the current graph */
	void OnDeterminismGraphTest() const;

private:
	/** Register PCG specific toolbar for the editor */
	void RegisterToolbar() const;

	/** Bind commands to delegates */
	void BindCommands();

	/** Bring up the find tab */
	void OnFind();

	/** Bring up the first free details view, or if they are all locked, the first details view */
	void OpenDetailsView();

	/** Called when a details view tab is closed */
	void OnDetailsViewTabClosed(TSharedRef<SDockTab> DockTab, int Index);

	/** Called when an attribute list view tab is closed */
	void OnAttributeListViewTabClosed(TSharedRef<SDockTab> DockTab, int Index);

	/** Called when a viewport view tab is closed */
	void OnViewportViewTabClosed(TSharedRef<SDockTab> DockTab, int Index);

	/** Enable/Disable automatic PCG node generation */
	void OnPauseAutomaticRegeneration_Clicked();
	/** Has the user paused automatic regeneration in the Graph Editor */
	bool IsAutomaticRegenerationPaused() const;

	/** Force a regeneration by invoking the graph notifications  */
	void OnForceGraphRegeneration_Clicked();

	/** Toggle node inspection state for selected nodes */
	void OnToggleInspected();
	/** Whether we can toggle inspection of selected nodes */
	bool CanToggleInspected() const;
	/** Whether selected nodes are inspected or not */
	ECheckBoxState GetInspectedCheckState() const;

	void UpdateAfterInspectedStackChanged();
	
	/** Toggle node enabled state for selected nodes */
	void OnToggleEnabled();
	/** Whether we can toggle enabled state of selected nodes */
	bool CanToggleEnabled() const;
	/** Whether selected nodes are enabled or not */
	ECheckBoxState GetEnabledCheckState() const;
	
	/** Toggle node debug state for selected nodes */
	void OnToggleDebug();
	/** Whether we can toggle debug state of selected nodes */
	bool CanToggleDebug() const;
	/** Whether selected nodes are being debugged or not */
	ECheckBoxState GetDebugCheckState() const;

	/** Enable node debug state for selected nodes and disable for others */
	void OnDebugOnlySelected();

	/** Disable node debug state for all nodes */
	void OnDisableDebugOnAllNodes();

	/** Cancels the current execution of the selected graph */
	void OnCancelExecution_Clicked();

	/** Returns true if inspected graph is currently scheduled or executing */
	bool IsCurrentlyGenerating() const;

	/** Returns true if the debug object tree tab is not currently open. */
	bool IsDebugObjectTreeTabClosed() const;

	/** Opens the debug object tree tab if it is not open already. */
	void OnOpenDebugObjectTreeTab_Clicked();

	/** Can determinism be tested on the selected node(s) */
	bool CanRunDeterminismNodeTest() const;
	/** Run the determinism test on the selected node(s) */
	void OnDeterminismNodeTest() const;

	/** Open details view for the PCG object being edited */
	void OnEditGraphSettings();
	/** Whether the PCG object being edited is opened in details view or not */
	bool IsEditGraphSettingsToggled() const;

	/** Open panel to view and edit the graph parameters. */
	void OnToggleGraphParamsPanel() const;
	/** Is the graph params panel is open. */
	bool IsToggleGraphParamsToggled() const;

	/** Select every node in the graph */
	void SelectAllNodes();
	/** Whether we can select every node */
	bool CanSelectAllNodes() const;

	/** Delete all selected nodes in the graph */
	void DeleteSelectedNodes();
	/** Whether we can delete all selected nodes */
	bool CanDeleteSelectedNodes() const;

	/** Copy all selected nodes in the graph */
	void CopySelectedNodes();
	/** Whether we can copy all selected nodes */
	bool CanCopySelectedNodes() const;

	/** Cut all selected nodes in the graph */
	void CutSelectedNodes();
	/** Whether we can cut all selected nodes */
	bool CanCutSelectedNodes() const;

	/** Paste nodes in the graph */
	void PasteNodes();
	/** Paste nodes in the graph at location*/
	void PasteNodesHere(const FVector2D& Location);
	/** Whether we can paste nodes */
	bool CanPasteNodes() const;

	/** Duplicate the currently selected nodes */
	void DuplicateNodes();
	/** Whether we are able to duplicate the currently selected nodes */
	bool CanDuplicateNodes() const;

	/** Exports node settings to assets */
	void OnExportNodes();

	/** Whether we are able to export the currently selected nodes */
	bool CanExportNodes() const;

	/** Converts instanced nodes to independent nodes */
	void OnConvertToStandaloneNodes();

	/** Whether we are able to convert the selected nodes to standalone */
	bool CanConvertToStandaloneNodes() const;

	/** Collapse the currently selected nodes in a subgraph */
	void OnCollapseNodesInSubgraph();
	/** Whether we can collapse nodes in a subgraph */
	bool CanCollapseNodesInSubgraph() const;

	/** User is attempting to add a dynamic source pin to a node */
	void OnAddDynamicInputPin();
	/** Whether the user can add a dynamic source pin to a node */
	bool CanAddDynamicInputPin() const;

	/** User is attempting to rename a node */
	void OnRenameNode();
	/** Whether the user can rename the selected node */
	bool CanRenameNode() const;

	/** Selects the associated usages of a given reroute declaration */
	void OnSelectNamedRerouteUsages();
	/** Whether the user can find the usages from the selection */
	bool CanSelectNamedRerouteUsages() const;

	/** Selects the associated declaration of a given reroute usage */
	void OnSelectNamedRerouteDeclaration();
	/** Whether the user can find the declaration from the selection */
	bool CanSelectNamedRerouteDeclaration() const;

	/** Jumps to source definition for the selected nodes. */
	void OnJumpToSource();

	/** Internal method that validates a few things (& logs errors) prior to executing actions. */
	bool InternalValidationOnAction();

	/** Finds editor graph node that matches the provided PCG node */
	UPCGEditorGraphNodeBase* GetEditorNode(const UPCGNode* InNode);

	void OnAlignTop();
	void OnAlignMiddle();
	void OnAlignBottom();
	void OnAlignLeft();
	void OnAlignCenter();
	void OnAlignRight();
	void OnStraightenConnections();
	void OnDistributeNodesH();
	void OnDistributeNodesV();
	void OnCreateComment();

	/** Create new graph editor widget */
	TSharedRef<SGraphEditor> CreateGraphEditorWidget();

	/** Create new palette widget */
	TSharedRef<SPCGEditorGraphNodePalette> CreatePaletteWidget();

	/** Create new debug object tree widget */
	TSharedRef<SPCGEditorGraphDebugObjectTree> CreateDebugObjectTreeWidget();

	/** Create new find widget */
	TSharedRef<SPCGEditorGraphFind> CreateFindWidget();

	/** Create new attributes widget */
	TSharedRef<SPCGEditorGraphAttributeListView> CreateAttributesWidget();

	/** Create a new determinism tab widget */
	TSharedRef<SPCGEditorGraphDeterminismListView> CreateDeterminismWidget();

	/** Create a new profiling tab widget */
	TSharedRef<SPCGEditorGraphProfilingView> CreateProfilingWidget();

	/** Create a new log capture tab widget */
	TSharedRef<SPCGEditorGraphLogView> CreateLogWidget();

	/** Create a new node source editor tab widget */
	TSharedRef<SPCGEditorNodeSource> CreateNodeSourceWidget();

	/** Create a new user graph parameters tab widget */
	TSharedRef<SPCGEditorGraphUserParametersView> CreateGraphParamsWidget();

	/** Create a new viewport widget */
	TSharedRef<SPCGEditorViewport> CreateViewportWidget();

	/** Called when the selection changes in the GraphEditor */
	void OnSelectedNodesChanged(const TSet<UObject*>& NewSelection);

	/** Called when the component inspected is generated/cleaned */
	void OnComponentGenerated(UPCGComponent* InComponent);

	/** Called to validate a node title change. */
	bool OnValidateNodeTitle(const FText& NewName, UEdGraphNode* GraphNode, FText& OutErrorMessage);

	/** Called when the title of a node is changed */
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	/**
	 * Called when a node is double clicked
	 *
	 * @param Node - The Node that was clicked
	 */
	void OnNodeDoubleClicked(UEdGraphNode* Node);

	/**
	 * Try to jump to a given class (if allowed)
	 *
	 * @param Class - The Class to jump to
	 */
	void JumpToDefinition(const UClass* Class) const;

	/** Called when a PCG component unregisters. */
	void OnComponentUnregistered(UPCGComponent* Component);

	/** Called when a component finishes executing. Useful for updating debugging tools/UIs. */
	void OnComponentGenerationDone(UPCGSubsystem* Subsystem, UPCGComponent* Component, EPCGGenerationStatus Status);

	/** Trigger any generation required to ensure debug display is up to date. */
	void UpdateDebugAfterComponentSelection(UPCGComponent* InOldComponent, UPCGComponent* InNewComponent, bool bNewComponentStartedInspecting);

	void RegisterDelegatesForWorld(UWorld* World);
	void UnregisterDelegatesForWorld(UWorld* World);

	void OnGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType);
	void OnNodeSourceCompiled(const UPCGNode* InNode, const FPCGCompilerDiagnostics& InDiagnostics);

	void OnMapChanged(UWorld* InWorld, EMapChangeType InMapChangedType);
	void OnPostPIEStarted(bool bIsSimulating);
	void OnEndPIE(bool bIsSimulating);
	void OnLevelActorDeleted(AActor* InActor);

	void RefreshViewsOnLevelChange();

	TSharedRef<SDockTab> SpawnTab_GraphEditor(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PropertyDetails(const FSpawnTabArgs& Args, int PropertyDetailsIndex);
	TSharedRef<SDockTab> SpawnTab_Palette(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_DebugObjectTree(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Attributes(const FSpawnTabArgs& Args, int AttributesIndex);
	TSharedRef<SDockTab> SpawnTab_Find(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Determinism(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Profiling(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Log(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_NodeSource(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_UserParams(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args, int ViewportIndex);

	FText GetDetailsTabLabel(int DetailsIndex);
	FText GetDetailsViewObjectName(int DetailsIndex);
	FText GetAttributesTabLabel(int AttributesIndex);
	FText GetViewportTabLabel(int ViewportIndex);

	static TSharedRef<FTabManager::FLayout> GetDefaultLayout();

	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TArray<TSharedPtr<SPCGEditorGraphDetailsView>> PropertyDetailsWidgets;
	TSharedPtr<SPCGEditorGraphNodePalette> PaletteWidget;
	TSharedPtr<SPCGEditorGraphDebugObjectTree> DebugObjectTreeWidget;
	TSharedPtr<SPCGEditorGraphFind> FindWidget;
	TArray<TSharedPtr<SPCGEditorGraphAttributeListView>> AttributesWidgets;
	TSharedPtr<SPCGEditorGraphDeterminismListView> DeterminismWidget;
	TSharedPtr<SPCGEditorGraphProfilingView> ProfilingWidget;
	TSharedPtr<SPCGEditorGraphLogView> LogWidget;
	TSharedPtr<SPCGEditorNodeSource> NodeSourceWidget;
	TSharedPtr<SPCGEditorGraphUserParametersView> UserParamsWidget;

	TSharedPtr<FUICommandList> GraphEditorCommands;

	TObjectPtr<UPCGGraph> PCGGraphBeingEdited = nullptr;
	UPCGEditorGraph* PCGEditorGraph = nullptr;

	TSoftObjectPtr<UPCGComponent> PCGComponentBeingInspected;
	// Implementation note: we'll keep the last valid component inspected so we don't un-inspect on spurious selection changes
	TSoftObjectPtr<UPCGComponent> LastValidPCGComponentBeingInspected;
	FPCGStack StackBeingInspected;
};
