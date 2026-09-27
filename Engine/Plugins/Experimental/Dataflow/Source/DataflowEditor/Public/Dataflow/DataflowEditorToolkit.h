// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorToolkit.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Misc/NotifyHook.h"
#include "GraphEditor.h"
#include "TickableEditorObject.h"
#include "Dataflow/DataflowSelectionView.h"
#include "Dataflow/DataflowCollectionSpreadSheet.h"
#include "Dataflow/DataflowConstructionViewport.h"
#include "Dataflow/DataflowSimulationViewport.h"
#include "Dataflow/DataflowContent.h"

class FDocumentTracker;
class FEditorViewportTabContent;
class IDetailsView;
class FTabManager;
class FTabInfo;
class IStructureDetailsView;
class IToolkitHost;
class UDataflow;
class UDataflowSubGraph;
class USkeletalMesh;
class SDataflowGraphEditor;
class FDataflowConstructionScene;
class FDataflowSimulationViewportClient;
class UDataflowBaseContent;
class FDataflowSimulationScene;
class FDataflowSkeletonView;
class FDataflowOutlinerView;
class UDataflowEditor;
class SDataflowMembersWidget;
class FDataflowSimulationSceneProfileIndexStorage;
class FDataflowOutputLog;
struct FDataflowPath;

enum class EDataflowEditorEvaluationMode : uint8;

namespace UE::Dataflow
{
	class FDataflowNodeDetailExtensionHandler;
}
namespace EMessageSeverity { enum Type : int; }

class DATAFLOWEDITOR_API FDataflowEditorToolkit final : public FBaseCharacterFXEditorToolkit, public FTickableEditorObject, public FNotifyHook, public FGCObject
{
	using FBaseCharacterFXEditorToolkit::ObjectScene;

public:

	explicit FDataflowEditorToolkit(UAssetEditor* InOwningAssetEditor);
	~FDataflowEditorToolkit();

	static bool CanOpenDataflowEditor(UObject* ObjectToEdit);
	static bool HasDataflowAsset(UObject* ObjectToEdit);
	static UDataflow* GetDataflowAsset(UObject* ObjectToEdit);
	static const UDataflow* GetDataflowAsset(const UObject* ObjectToEdit);
	bool IsSimulationDataflowAsset() const;
	FName GetGraphLogName() const;
	void LogMessage(const EMessageSeverity::Type Severity, const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Message) const;

	/** Editor dataflow content accessors */
	const TObjectPtr<UDataflowBaseContent>& GetEditorContent() const;
	TObjectPtr<UDataflowBaseContent>& GetEditorContent();

	/** Terminal dataflow contents accessors */
	const TArray<TObjectPtr<UDataflowBaseContent>>& GetTerminalContents() const;
	TArray<TObjectPtr<UDataflowBaseContent>>& GetTerminalContents();
	
	/** Dataflow graph editor accessor */
	const TSharedPtr<SDataflowGraphEditor> GetDataflowGraphEditor() const { return GraphEditor; }

	// IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Dataflow preview scenes accessor */
	const TSharedPtr<FDataflowSimulationScene>& GetSimulationScene() const {return SimulationScene;}
	FDataflowConstructionScene* GetConstructionScene() const;
	const TSharedPtr<FDataflowSimulationSceneProfileIndexStorage>& GetSimulationSceneProfileIndexStorage() const { return SimulationSceneProfileIndexStorage; }

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FDataflowEditorToolkit");
	}
	// End of FSerializableObject interface

	void OpenSubGraphTab(FName SubGraphName);
	void OpenSubGraphTab(const UDataflowSubGraph* SubGraph);
	void CloseSubGraphTab(const UDataflowSubGraph* SubGraph);
	void ReOpenSubGraphTab(const UDataflowSubGraph* SubGraph);
	void SetSubGraphTabActiveState(TSharedPtr<SDataflowGraphEditor> SubGraphEditor, bool bActive);
	UDataflowSubGraph* GetSubGraph(const FGuid& SubGraphGuid) const;
	UDataflowSubGraph* GetSubGraph(FName SubGraphName) const;

	const FString& GetDebugDrawOverlayString() const;
	
	/** Get the toolkit evaluation mode */
	const EDataflowEditorEvaluationMode& GetEvaluationMode() const { return EvaluationMode; }

protected:

	UDataflowEditor* DataflowEditor = nullptr;

	// List of dataflow actions callbacks
	void OnPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	bool OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* GraphNode, FText& OutErrorMessage);
	void OnNodeTitleCommitted(const FText& InNewText, ETextCommit::Type InCommitType, UEdGraphNode* GraphNode);
	void OnNodeDoubleClicked(UEdGraphNode* ClickedNode);
	void OnNodeSelectionChanged(const TSet<UObject*>& NewSelection);
	void OnEvaluateSelectedNodes(const TSet<UObject*>& SelectedNodes);
	void OnNodeInvalidated(UDataflow& DataflowAsset, FDataflowNode& Node);
	void OnNodeDeleted(const TSet<UObject*>& NewSelection);
	void OnNodeSingleClicked(UObject* ClickedNode) const;
	void OnAssetPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	void OnConstructionViewSelectionChanged(const TArray<UPrimitiveComponent*>& SelectedComponents, const TArray<FDataflowBaseElement*>& SelectedElements);
	void OnSimulationViewSelectionChanged(const TArray<UPrimitiveComponent*>& SelectedComponents, const TArray<FDataflowBaseElement*>& SelectedElements);
	// Callback to remove the closed one from the listener views
	void OnTabClosed(TSharedRef<SDockTab> Tab);

	// Node evaluation
	void EvaluateTerminalNode(const FDataflowTerminalNode& TerminalNode);
	void EvaluateNode(const FDataflowNode* Node, const FDataflowOutput* Output, UE::Dataflow::FTimestamp& InOutTimestamp);
	void EvaluateGraph();
	void RefreshViewsIfNeeded(bool bForce = false);
	void OnNodeBeginEvaluate(const FDataflowNode* Node, const FDataflowOutput* Output);
	void OnNodeFinishEvaluate(const FDataflowNode* Node, const FDataflowOutput* Output);
	void OnBeginEvaluate();
	void OnFinishEvaluate();
	void OnOutputLogMessageTokenClicked(const FString TokenString);
	void OnContextHasInfo(const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Info);
	void OnContextHasWarning(const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Warning);
	void OnContextHasError(const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Error);

private:
	
	// Spawning of all the additional tabs (viewport,details ones are coming from the base asset toolkit)
	TSharedRef<SDockTab> SpawnTab_GraphCanvas(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SubGraphTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_NodeDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SkeletonView(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_OutlinerView(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SelectionView(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_CollectionSpreadSheet(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_AssetDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SimulationViewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PreviewScene(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SimulationVisualization(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_MembersWidget(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_OutputLog(const FSpawnTabArgs& Args);

	// FTickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;

	// FBaseCharacterFXEditorToolkit interface
	virtual FEditorModeID GetEditorModeId() const override;
	virtual void InitializeEdMode(UBaseCharacterFXEditorMode* EdMode) override;
	virtual void CreateEditorModeUILayer() override;

	// FAssetEditorToolkit interface
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void PostInitAssetEditor() override;
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;

	// FBaseAssetToolkit interface
	virtual void CreateWidgets() override;
	virtual AssetEditorViewportFactoryFunction GetViewportDelegate() override;
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;
	virtual void CreateEditorModeManager() override;

	// FNotifyHook
	virtual void NotifyPreChange(class FEditPropertyChain* PropertyAboutToChange) override;

	// List of all the tab names ids that will be used to identify the editor widgets
	static const FName GraphCanvasTabId;
	static const FName SubGraphCanvasTabId;
	static const FName NodeDetailsTabId;
	static const FName SkeletonViewTabId;
	static const FName OutlinerViewTabId;
	static const FName SelectionViewTabId_1;
	static const FName SelectionViewTabId_2;
	static const FName SelectionViewTabId_3;
	static const FName SelectionViewTabId_4;
	static const FName CollectionSpreadSheetTabId_1;
	static const FName CollectionSpreadSheetTabId_2;
	static const FName CollectionSpreadSheetTabId_3;
	static const FName CollectionSpreadSheetTabId_4;
	static const FName SimulationViewportTabId;
	static const FName PreviewSceneTabId;
	static const FName SimulationVisualizationTabId;
	static const FName MembersWidgetTabId;
	static const FName OutputLogTabId;

	// List of all the widgets shared ptr that will be built in the editor
	TSharedPtr<SDataflowConstructionViewport> DataflowConstructionViewport;
	TSharedPtr<SDataflowSimulationViewport> DataflowSimulationViewport;
	TSharedPtr<SDataflowGraphEditor> GraphEditor;
	TSharedPtr<SDockTab> GraphEditorTab;
	TSharedPtr<IStructureDetailsView> NodeDetailsEditor;
	TSharedPtr<SDataflowMembersWidget> MembersWidget;
	TSharedPtr<UE::Dataflow::FDataflowNodeDetailExtensionHandler> NodeDetailsExtensionHandler;
	TSharedPtr<FDataflowSkeletonView> SkeletonEditorView;
	TSharedPtr<FDataflowOutlinerView> DataflowOutlinerView;
	TSharedPtr<IDetailsView> AssetDetailsEditor;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_1;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_2;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_3;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_4;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_1;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_2;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_3;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_4;
	TSharedPtr<SWidget> AdvancedPreviewSettingsWidget;
	TSharedPtr<SWidget> SimulationVisualizationWidget;
	TSharedPtr<FDataflowOutputLog> DataflowOutputLog;

	/** Customize preview scene with editor/terminal contents */
	TSharedRef<class IDetailCustomization> CustomizePreviewSceneDescription() const;

	TSharedRef<class IDetailCustomization> CustomizeAssetViewer() const;

	// Utility factory functions to build the widgets
	TSharedRef<SDataflowGraphEditor> CreateGraphEditorWidget(UEdGraph* GraphToEdit, TSharedPtr<IStructureDetailsView> PropertiesEditor);
    TSharedPtr<IDetailsView> CreateAssetDetailsEditorWidget(const TArray<UObject*>& ObjectsToEdit);
	TSharedPtr<SWidget> CreateSimulationVisualizationWidget();
    TSharedPtr<IStructureDetailsView> CreateNodeDetailsEditorWidget(UObject* ObjectToEdit);
	TSharedPtr<SDataflowMembersWidget> CreateDataflowMembersWidget();
	TSharedRef<SGraphEditor> CreateSubGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UDataflowSubGraph* InGraph);

	void AddEvaluationWidget(FToolMenuSection& Section);
	TSharedRef<SWidget> GenerateEvaluationOptionsMenu();
	FSlateIcon GetEvaluationStatusImage() const;
	bool IsGraphDirty() const;
	bool IsEvaluateButtonEnabled() const;

	void SetEvaluateGraphMode(EDataflowEditorEvaluationMode Mode);
	void ClearGraphCache();
	bool CanClearGraphCache() const;
	void TogglePerfData();
	bool IsPerfDataEnabled() const;
	void ToggleAsyncEvaluation();
	bool IsAsyncEvaluationEnabled() const;

	/** Create the simulation viewport client */
	void CreateSimulationViewportClient();

	void SetDataflowPathFromNodeAndOutput(const FDataflowNode* Node, const FDataflowOutput* Output, FDataflowPath& OutPath) const;

	void RegisterContextHandlers();
	void UnregisterContextHandlers();

	/** Update the debug draw based on a change of currently selected or pinned nodes */
	void UpdateDebugDraw();

	// List of editor commands used  for the dataflow asset
	TSharedPtr<FUICommandList> GraphEditorCommands;

	// List of selection view / collection spreadsheet widgets that are listening to any changed in the graph
	TArray<IDataflowViewListener*> ViewListeners;

	// Graph delegates used to update the UI
	FDelegateHandle OnSelectionChangedMulticastDelegateHandle;
    FDelegateHandle OnNodeDeletedMulticastDelegateHandle;
	FDelegateHandle OnEvaluateSelectedNodesDelegateHandle;
    FDelegateHandle OnFinishedChangingPropertiesDelegateHandle;
	FDelegateHandle OnFinishedChangingAssetPropertiesDelegateHandle;
	FDelegateHandle OnConstructionSelectionChangedDelegateHandle;
	FDelegateHandle OnSimulationSelectionChangedDelegateHandle;
	FDelegateHandle OnSimulationSceneChangedDelegateHandle;

	// Delegates to communicate with Context
	FDelegateHandle OnNodeBeginEvaluateMulticastDelegateHandle;
	FDelegateHandle OnNodeFinishEvaluateMulticastDelegateHandle;
	FDelegateHandle OnOutputLogMessageTokenClickedDelegateHandle;

	FDelegateHandle OnContextHasInfoDelegateHandle;
	FDelegateHandle OnContextHasWarningDelegateHandle;
	FDelegateHandle OnContextHasErrorDelegateHandle;

	// The currently selected set of dataflow nodes. 
	UPROPERTY()
	TSet< TObjectPtr<UObject> > SelectedDataflowNodes;

	// The most recently selected dataflow node.
	UPROPERTY()
	TObjectPtr<UDataflowEdNode> PrimarySelection;
	
	/** Pointer to the construction viewport scene. Note this is an alias of ObjectScene in FBaseCharacterFXEditorToolkit but with the specific type */
	FDataflowConstructionScene* ConstructionScene;

	/** PreviewScene showing the objects being simulated */
	TSharedPtr<FDataflowSimulationScene> SimulationScene;

	TSharedPtr<FDataflowSimulationSceneProfileIndexStorage> SimulationSceneProfileIndexStorage;

	/** The editor mode manager used by the simulation preview scene */
	TSharedPtr<FEditorModeTools> SimulationModeManager;

	/** Simulation tab content */
	TSharedPtr<class FEditorViewportTabContent> SimulationTabContent;

	/** Simulation viewport delegate */
	AssetEditorViewportFactoryFunction SimulationViewportDelegate;

	/** Simulation Viewport client */
	TSharedPtr<FDataflowSimulationViewportClient> SimulationViewportClient;

	/** Simulation default layout */
	TSharedPtr<FTabManager::FLayout> SimulationDefaultLayout;
	
	/** Simulation default layout */
	TSharedPtr<FTabManager::FLayout> ConstructionDefaultLayout;

	/** Cached value of the p.Dataflow.EnableGraphEval cvar, to avoid calling FindConsoleVariable too often */
	bool bDataflowEnableGraphEval;

	EDataflowEditorEvaluationMode EvaluationMode;

	/** Delegate for updating the cached value of p.Dataflow.EnableGraphEval */
	FDelegateHandle GraphEvalCVarChangedDelegateHandle;

	TWeakPtr<SDataflowGraphEditor> ActiveSubGraphEditorWeakPtr;

	/** Document tracker for dynamic tabs ( like subgraphs ) */
	TSharedPtr<FDocumentTracker> DocumentManager;

	TSet<FGuid> NodesToEvaluateOnTick;

	FDateTime GraphEvaluationBegin;
	FDateTime GraphEvaluationFinished;
	bool bViewsNeedRefresh = false;

	FString DebugDrawOverlayString;
};
