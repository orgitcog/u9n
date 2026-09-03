// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMEditorGraphExplorerTabSummoner.h"
#include "RigVMNewEditorMode.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "RigVMModel/RigVMController.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "RigVMModel/RigVMNotifications.h"
#include "Widgets/SRigVMEditorGraphExplorer.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"
#include "GraphEditor.h"
#include "Misc/NotifyHook.h"
#include "TickableEditorObject.h"
#include "EditorUndoClient.h"
#include "RigVMEditorModule.h"
#include "SNodePanel.h"
#include "Editor/RigVMDetailsViewWrapperObject.h"

#define LOCTEXT_NAMESPACE "RigVMEditor"

class FDocumentTracker;
class SRigVMEditorGraphExplorer;
class URigVM;
class FTabInfo;
class FRigVMEditorBase;
class URigVMController;
class URigVMHost;
class FTransaction;
class URigVMBlueprint;
class SGraphEditor;
class FPreviewScene;

DECLARE_MULTICAST_DELEGATE_TwoParams(FRigVMEditorClosed, const IRigVMEditor*, URigVMBlueprint*);


/* Enums to use when grouping the blueprint members in the list panel. The order here will determine the order in the list */
namespace RigVMNodeSectionID
{
	// Keep the values as they are defined in NodeSectionID, which is defined in BlueprintEditor.h
	// TODO: Once there is no need for FRigVMLegacyEditor, improve the definition of this enum, including a uint8 definition
	enum Type
	{
		NONE = 0,
		GRAPH = 1,					// Graph
		FUNCTION = 4,				// Functions
		VARIABLE = 8,				// Variables
		LOCAL_VARIABLE = 12			// Local variables
	};
};

struct FRigVMEditorModes
{
	// Mode constants
	RIGVMEDITOR_API static inline const FLazyName RigVMEditorMode = FLazyName(TEXT("RigVM"));
	static FText GetLocalizedMode(const FName InMode)
	{
		static TMap< FName, FText > LocModes;

		if (LocModes.Num() == 0)
		{
			LocModes.Add(RigVMEditorMode, NSLOCTEXT("RigVMEditorModes", "RigVMEditorMode", "RigVM"));
		}

		check(InMode != NAME_None);
		const FText* OutDesc = LocModes.Find(InMode);
		check(OutDesc);
		return *OutDesc;
	}
private:
	FRigVMEditorModes() {}
};

struct FRigVMCustomDebugObject
{
public:
	// Custom object to include, regardless of the current debugging World
	UObject* Object;

	// Override for the object name (if not empty)
	FString NameOverride;

public:
	FRigVMCustomDebugObject()
		: Object(nullptr)
	{
	}

	FRigVMCustomDebugObject(UObject* InObject, const FString& InLabel)
		: Object(InObject)
		, NameOverride(InLabel)
	{
	}
};

class RIGVMEDITOR_API IRigVMEditor
{
public:

	virtual TSharedPtr<FAssetEditorToolkit> GetHostingApp() = 0;
	virtual URigVMBlueprint* GetRigVMBlueprint() const = 0;
	virtual const TArray< UObject* >* GetObjectsCurrentlyBeingEdited() const = 0;
	virtual URigVMHost* GetRigVMHost() const = 0;

	virtual TSharedPtr<FTabManager> GetTabManager() = 0;
	virtual FName GetGraphExplorerWidgetID() = 0;
	virtual TSharedPtr<class SRigVMDetailsInspector> GetRigVMInspector() const = 0;
	virtual TSharedPtr<SRigVMEditorGraphExplorer> GetGraphExplorerWidget() = 0;
#if WITH_RIGVMLEGACYEDITOR
	virtual TSharedPtr<class SKismetInspector> GetKismetInspector() const = 0;
#endif
	
	virtual bool GetIsContextSensitive() = 0;
	virtual void SetIsContextSensitive(const bool bIsContextSensitive) = 0;

	virtual void SetGraphExplorerWidget(TSharedPtr<SRigVMEditorGraphExplorer> InWidget) = 0;
	
	virtual void GetPinTypeSelectorFilters(TArray<TSharedPtr<class IPinTypeSelectorFilter>>& OutFilters) const = 0;

	DECLARE_EVENT(IRigVMEditor, FOnRefreshEvent);
	virtual FOnRefreshEvent OnRefresh() = 0;
	virtual void ForceEditorRefresh(ERefreshRigVMEditorReason::Type Reason = ERefreshRigVMEditorReason::UnknownReason) = 0;

	DECLARE_EVENT_OneParam(IRigVMEditor, FPreviewHostUpdated, IRigVMEditor*);
	virtual FPreviewHostUpdated& OnPreviewHostUpdated()  = 0;

	virtual FRigVMEditorClosed& OnEditorClosed() = 0;
	virtual UEdGraph* GetFocusedGraph() const = 0;
	virtual URigVMGraph* GetFocusedModel() const = 0;
	virtual FNotifyHook* GetNotifyHook() = 0;
	virtual TWeakPtr<class SGraphEditor> GetFocusedGraphEditor() = 0;

	virtual bool InEditingMode() const = 0;
	virtual bool IsEditable(UEdGraph* InGraph) const = 0;
	
	virtual void OnGraphNodeDropToPerform(TSharedPtr<FDragDropOperation> InDragDropOp, UEdGraph* InGraph, const FVector2f& InNodePosition, const FVector2f& InScreenPosition) = 0;
	virtual TSharedRef<FUICommandList> GetToolkitCommands() = 0;

	virtual bool SelectLocalVariable(const UEdGraph* Graph, const FName& VariableName) = 0;
	virtual void JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename = false) = 0;
	virtual void OnAddNewLocalVariable() = 0;
	virtual bool CanAddNewLocalVariable() const = 0;
	virtual void OnAddNewVariable() = 0;
	virtual void OnPasteNewLocalVariable(const FBPVariableDescription& VariableDescription) = 0;
	virtual void AddNewFunctionVariant(const UEdGraph* InOriginalFunction) = 0;
	virtual TSharedPtr<SDockTab> OpenDocument(const UObject* DocumentID, FDocumentTracker::EOpenDocumentCause Cause) = 0;
	
	// Type of new document/graph being created by a menu item
	enum ECreatedDocumentType
	{
		CGT_NewVariable,
		CGT_NewFunctionGraph,
		CGT_NewMacroGraph,
		CGT_NewAnimationLayer,
		CGT_NewEventGraph,
		CGT_NewLocalVariable
	};
	virtual void OnNewDocumentClicked(ECreatedDocumentType GraphType) = 0;
	virtual void SummonSearchUI(bool bSetFindWithinBlueprint, FString NewSearchTerms = FString(), bool bSelectFirstResult = false) = 0;
	
	virtual void GetDebugObjects(TArray<FRigVMCustomDebugObject>& DebugList) const = 0;
	virtual bool OnlyShowCustomDebugObjects() const = 0;
	virtual FString GetCustomDebugObjectLabel(UObject* ObjectBeingDebugged) const = 0;

	virtual TSharedPtr<SGraphEditor> OpenGraphAndBringToFront(UEdGraph* Graph, bool bSetFocus = true) = 0;
	virtual void ZoomToSelection_Clicked() = 0;

	virtual void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject) = 0;
	virtual void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) = 0;
	virtual FPreviewScene* GetPreviewScene() = 0;
	
protected:
	
	virtual TSharedRef<IRigVMEditor> SharedRef() = 0;
	virtual TSharedRef<const IRigVMEditor> SharedRef() const = 0;

	
	virtual const TSharedPtr<FAssetEditorToolkit> GetHostingApp() const = 0;
	virtual void InitAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const FName AppIdentifier, const TSharedRef<FTabManager::FLayout>& StandaloneDefaultLayout, const bool bCreateDefaultStandaloneMenu, const bool bCreateDefaultToolbar, const TArray<UObject*>& ObjectsToEdit, const bool bInIsToolbarFocusable = false, const bool bInUseSmallToolbarIcons = false, const TOptional<EAssetOpenMethod>& InOpenMethod = TOptional<EAssetOpenMethod>()) = 0;
	virtual void CreateEditorToolbar() = 0;
	virtual void CommonInitialization(const TArray<UBlueprint*>& InitBlueprints, bool bShouldOpenInDefaultsMode) = 0;
	virtual TSharedPtr<FDocumentTracker> GetDocumentManager() const = 0;
	virtual void AddApplicationMode(FName ModeName, TSharedRef<FApplicationMode> Mode) = 0;
	virtual void RegenerateMenusAndToolbars() = 0;
	virtual void SetCurrentMode(FName NewMode) = 0;
	virtual FEditorModeTools& GetToolkitEditorModeManager() const = 0;
	virtual void PostLayoutBlueprintEditorInitialization() = 0;
	virtual bool FindOpenTabsContainingDocument(const UObject* DocumentID, /*inout*/ TArray< TSharedPtr<SDockTab> >& Results) = 0;
	virtual void CloseDocumentTab(const UObject* DocumentID) = 0;
	virtual TSharedPtr<FApplicationMode> CreateEditorMode() = 0;
	virtual const FName GetEditorAppName() const = 0;
	virtual const TArray< UObject* >& GetEditingBlueprints() const = 0;
	virtual const TSharedRef<IToolkitHost> GetToolkitHost() const = 0;
	virtual bool IsHosted() const = 0;
	virtual void BringToolkitToFrontImpl() = 0;
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) = 0;
	virtual TWeakPtr<FDocumentTabFactory> GetGraphEditorTabFactory() const = 0;
	virtual bool TransactionObjectAffectsBlueprint(UObject* InTransactedObject) = 0;
	virtual FEdGraphPinType GetLastPinTypeUsed() = 0;
	virtual void LogSimpleMessage(const FText& MessageText) = 0;
	virtual void RenameNewlyAddedAction(FName InActionName) = 0;
	virtual FGraphPanelSelectionSet GetSelectedNodes() const = 0;
	virtual void SetUISelectionState(FName SelectionOwner) = 0;
	virtual void AnalyticsTrackNodeEvent(UBlueprint* Blueprint, UEdGraphNode *GraphNode, bool bNodeDelete = false) const = 0;
	virtual void PostUndo(bool bSuccess) = 0;
	virtual void PostRedo(bool bSuccess) = 0;
	virtual UEdGraphPin* GetCurrentlySelectedPin() const = 0;
	virtual void CreateDefaultCommands() = 0;
	virtual TSharedRef<SGraphEditor> CreateGraphEditorWidget(TSharedRef<class FTabInfo> InTabInfo, class UEdGraph* InGraph) = 0;
	virtual void CompileImpl() = 0;
	virtual void SaveAsset_Execute_Impl() = 0;
	virtual void SaveAssetAs_Execute_Impl() = 0;
	virtual bool IsGraphInCurrentBlueprint(const UEdGraph* InGraph) const = 0;
	virtual bool IsEditableImpl(UEdGraph* InGraph) const = 0;
	virtual void JumpToNode(const class UEdGraphNode* Node, bool bRequestRename = false) = 0;
	virtual void JumpToPin(const class UEdGraphPin* Pin) = 0;
	virtual void AddToolbarExtender(TSharedPtr<FExtender> Extender) = 0;
	virtual void RemoveToolbarExtender(TSharedPtr<FExtender> Extender) = 0;
	virtual void AddMenuExtender(TSharedPtr<FExtender> Extender) = 0;
	virtual void RemoveMenuExtender(TSharedPtr<FExtender> Extender) = 0;
	virtual TSharedPtr<class IMessageLogListing> GetCompilerResultsListing() = 0;
	virtual void OnBlueprintChangedInnerImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled = false) = 0;
	virtual void RefreshEditorsImpl(ERefreshRigVMEditorReason::Type Reason = ERefreshRigVMEditorReason::UnknownReason) = 0;
	virtual void SetupGraphEditorEventsImpl(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents) = 0;
	virtual FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2f& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed) = 0;
	virtual void AddReferencedObjectsImpl(FReferenceCollector& Collector) = 0;
	virtual FGraphAppearanceInfo GetGraphAppearanceImpl(class UEdGraph* InGraph) const = 0;
	virtual void NotifyPreChangeImpl( FProperty* PropertyAboutToChange ) = 0;
	virtual void NotifyPostChangeImpl( const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) = 0;
	virtual FName GetSelectedVariableName() = 0;
	virtual bool IsNodeTitleVisible(const UEdGraphNode* Node, bool bRequestRename) = 0;
	virtual void EditClassDefaults_Clicked() = 0;
	virtual void EditGlobalOptions_Clicked() = 0;
	virtual void TryInvokingDetailsTab(bool bFlash = true) = 0;
	virtual void RefreshInspector() = 0;
	virtual void RefreshStandAloneDefaultsEditor() = 0;
	virtual void RestoreEditedObjectState() = 0;
	virtual void SetupViewForBlueprintEditingMode() = 0;
	virtual void RegisterToolbarTab(const TSharedRef<class FTabManager>& TabManager) = 0;
	virtual void AddCompileWidget(FToolBarBuilder& ToolbarBuilder) = 0;
	virtual void AddSettingsAndDefaultWidget(FToolBarBuilder& ToolbarBuilder) = 0;
	virtual void AddSelectedDebugObjectWidget(FToolBarBuilder& ToolbarBuilder) = 0;
	virtual void AddAutoCompileWidget(FToolBarBuilder& ToolbarBuilder) = 0;
};

class RIGVMEDITOR_API FRigVMEditorBase : public IRigVMEditor
{
public:
	
	/**
	 * Edits the specified asset(s)
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	InRigVMBlueprint	The blueprint object to start editing.
	 */
	virtual void InitRigVMEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class URigVMBlueprint* InRigVMBlueprint);

	static FRigVMEditorBase* GetFromAssetEditorInstance(IAssetEditorInstance* Instance);
	// returns the blueprint being edited
	virtual URigVMBlueprint* GetRigVMBlueprint() const override;
	
	void HandleJumpToHyperlink(const UObject* InSubject);

	void Compile();
	bool IsCompilingEnabled() const;

	void DeleteSelectedNodes();
	bool CanDeleteNodes() const;
	void CopySelectedNodes();
	bool CanCopyNodes() const;
	void PasteNodes();
	bool CanPasteNodes() const;
	void CutSelectedNodes();
	bool CanCutNodes() const;
	void DuplicateNodes();
	bool CanDuplicateNodes() const;
	
	void OnStartWatchingPin();
	bool CanStartWatchingPin() const;
	void OnStopWatchingPin();
	bool CanStopWatchingPin() const;

	virtual FOnRefreshEvent OnRefresh() override { return OnRefreshEvent; }

	FText GetGraphDecorationString(UEdGraph* InGraph) const;
	virtual bool IsEditable(UEdGraph* InGraph) const override;

	virtual void OnGraphNodeDropToPerform(TSharedPtr<FDragDropOperation> InDragDropOp, UEdGraph* InGraph, const FVector2f& InNodePosition, const FVector2f& InScreenPosition) override;

	/**
	 * Util for finding a glyph and color for a variable.
	 *
	 * @param Property       The variable's property
	 * @param IconColorOut      The resulting color for the glyph
	 * @param SecondaryBrushOut The resulting secondary glyph brush (used for Map types)
	 * @param SecondaryColorOut The resulting secondary color for the glyph (used for Map types)
	 * @return					The resulting glyph brush
	 */
	static FSlateBrush const* GetVarIconAndColorFromProperty(const FProperty* Property, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut);

	/**
	* Util for finding a glyph and color for a variable.
	*
	* @param PinType       The variable's pin type
	* @param IconColorOut      The resulting color for the glyph
	* @param SecondaryBrushOut The resulting secondary glyph brush (used for Map types)
	* @param SecondaryColorOut The resulting secondary color for the glyph (used for Map types)
	* @return					The resulting glyph brush
	*/
	static FSlateBrush const* GetVarIconAndColorFromPinType(const FEdGraphPinType& PinType, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut);
	
protected:
	
	FRigVMEditorBase();
	virtual ~FRigVMEditorBase(){}

	void UnbindEditor();
	

	void HandleAssetRequestedOpen(UObject* InObject);
	void HandleAssetRequestClose(UObject* InObject, EAssetEditorCloseReason InReason);
	bool bRequestedReopen = false;

	virtual const FName GetEditorModeName() const;

	// FBlueprintEditor interface
	UBlueprint* GetBlueprintObj() const;
	virtual bool InEditingMode() const override;
	TSubclassOf<UEdGraphSchema> GetDefaultSchemaClass() const;
	void OnGraphEditorFocused(const TSharedRef<SGraphEditor>& InGraphEditor);

	//  FTickableEditorObject Interface
	void Tick(float DeltaTime);

	// IToolkit Interface
	void BringToolkitToFront();
	FName GetToolkitFName() const;
	FName GetToolkitContextFName() const;
	FText GetBaseToolkitName() const;
	FText GetToolkitToolTipText() const;
	FString GetWorldCentricTabPrefix() const;
	FLinearColor GetWorldCentricTabColorScale() const;	
	void InitToolMenuContextImpl(FToolMenuContext& MenuContext);

	// BlueprintEditor interface
	bool TransactionObjectAffectsBlueprintImpl(UObject* InTransactedObject);
	virtual bool CanAddNewLocalVariable() const override;
	virtual void OnAddNewLocalVariable() override;
	virtual void OnPasteNewLocalVariable(const FBPVariableDescription& VariableDescription) override;


	bool IsNativeParentClassCodeLinkEnabled() const { return false; }
	bool ReparentBlueprint_IsVisible() const { return false; }
	FReply OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2f& InPosition, UEdGraph* InGraph);
	bool ShouldLoadBPLibrariesFromAssetRegistry() { return false; }
	bool JumpToHyperlinkImpl(const UObject* ObjectReference, bool bRequestRename = false);
	bool ShouldOpenGraphByDefault() const { return true; }
	virtual void AddNewFunctionVariant(const UEdGraph* InOriginalFunction) override;

	// FEditorUndoClient Interface
	void PostUndoImpl(bool bSuccess);
	void PostRedoImpl(bool bSuccess);

	virtual void PostTransaction(bool bSuccess, const FTransaction* Transaction, bool bIsRedo);


	// IToolkitHost Interface
	void OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit);
	void OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit);

	//  FTickableEditorObject Interface
	TStatId GetStatId() const;

	// returns the currently debugged / viewed host
	virtual URigVMHost* GetRigVMHost() const override;

	virtual UObject* GetOuterForHost() const;

	// returns the class to use for detail wrapper objects (UI shim layer)
	virtual UClass* GetDetailWrapperClass() const;

	// allows the editor to fill an empty graph
	virtual void CreateEmptyGraphContent(URigVMController* InController) {}

	virtual FPreviewHostUpdated& OnPreviewHostUpdated() override { return PreviewHostUpdated;  }

	virtual FRigVMEditorClosed& OnEditorClosed() override { return RigVMEditorClosedDelegate; }


	/** Get the toolbox hosting widget */
	TSharedRef<SBorder> GetToolbox() { return Toolbox.ToSharedRef(); }

	virtual bool SelectLocalVariable(const UEdGraph* Graph, const FName& VariableName) override;


	enum ERigVMEditorExecutionModeType
	{
		ERigVMEditorExecutionModeType_Release,
		ERigVMEditorExecutionModeType_Debug
	};

	// FBlueprintEditor Interface
	void CreateDefaultCommandsImpl();
	void OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList);

	void SaveAsset_Execute();
	void SaveAssetAs_Execute();
	bool IsInAScriptingMode() const { return true; }
	virtual void OnNewDocumentClicked(ECreatedDocumentType GraphType) override;
	bool IsSectionVisibleImpl(RigVMNodeSectionID::Type InSectionID) const;
	bool AreEventGraphsAllowed() const;
	bool AreMacrosAllowed() const;
	bool AreDelegatesAllowed() const;
	bool NewDocument_IsVisibleForTypeImpl(ECreatedDocumentType GraphType) const;
	FGraphAppearanceInfo GetGraphAppearance(class UEdGraph* InGraph) const;
	void OnActiveTabChanged( TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated );
	void OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection);
	void OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled);
	virtual void ForceEditorRefresh(ERefreshRigVMEditorReason::Type Reason = ERefreshRigVMEditorReason::UnknownReason) override;
	void SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents);
	void FocusInspectorOnGraphSelection(const TSet<class UObject*>& NewSelection, bool bForceRefresh = false);
#if WITH_RIGVMLEGACYEDITOR
	virtual TSharedPtr<SKismetInspector> GetKismetInspector() const override { return nullptr; }
#endif

	virtual void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject) override;
	UE_DEPRECATED(5.4, "Please use HandleVMCompiledEvent with ExtendedExecuteContext param.")
	virtual void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM) {}
	virtual void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM, FRigVMExtendedExecuteContext& InContext);
	virtual void HandleVMExecutedEvent(URigVMHost* InHost, const FName& InEventName);
	void HandleVMExecutionHalted(const int32 InstructionIndex, UObject* InNode, const FName& InEntryName);
	void SetHaltedNode(URigVMNode* Node);
	
	// FNotifyHook Interface
	void NotifyPreChange(FProperty* PropertyAboutToChange);
	void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged);
	/** delegate for changing property */
	virtual void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) override;
	void OnPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent);
	virtual void OnWrappedPropertyChangedChainEvent(URigVMDetailsViewWrapperObject* InWrapperObject, const FString& InPropertyPath, FPropertyChangedChainEvent& InPropertyChangedChainEvent);
	void OnRequestLocalizeFunctionDialog(FRigVMGraphFunctionIdentifier& InFunction, URigVMController* InTargetController, IRigVMGraphFunctionHost* InTargetFunctionHost, bool bForce);
	FRigVMController_BulkEditResult OnRequestBulkEditDialog(URigVMBlueprint* InBlueprint, URigVMController* InController, URigVMLibraryNode* InFunction, ERigVMControllerBulkEditType InEditType);
	bool OnRequestBreakLinksDialog(TArray<URigVMLink*> InLinks);
	TRigVMTypeIndex OnRequestPinTypeSelectionDialog(const TArray<TRigVMTypeIndex>& InTypes);

	bool UpdateDefaultValueForVariable(FBPVariableDescription& InVariable, bool bUseCDO);

	URigVMController* ActiveController;

	/** Push a newly compiled/opened host to the editor */
	virtual void UpdateRigVMHost();
	virtual void UpdateRigVMHost_PreClearOldHost(URigVMHost* InPreviousHost) {};

	/** Update the name lists for use in name combo boxes */
	virtual void CacheNameLists();

	// FGCObject Interface
	void AddReferencedObjects( FReferenceCollector& Collector );

	virtual void BindCommands();

	void ToggleAutoCompileGraph();
	bool IsAutoCompileGraphOn() const;
	bool CanAutoCompileGraph() const { return true; }
	void ToggleEventQueue();
	void ToggleExecutionMode();
	TSharedRef<SWidget> GenerateEventQueueMenuContent();
	TSharedRef<SWidget> GenerateExecutionModeMenuContent();
	virtual FMenuBuilder GenerateBulkEditMenu();
	TSharedRef<SWidget> GenerateBulkEditMenuContent();
	virtual void GenerateEventQueueMenuContent(FMenuBuilder& MenuBuilder);

	/** Wraps the normal blueprint editor's action menu creation callback */
	FActionMenuContent HandleCreateGraphActionMenu(UEdGraph* InGraph, const FVector2f& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	/** Undo Action**/
	void UndoAction();

	/** Redo Action **/
	void RedoAction();
	
	void OnCreateComment();

	bool IsDetailsPanelRefreshSuspended() const { return bSuspendDetailsPanelRefresh; }
	bool& GetSuspendDetailsPanelRefreshFlag() { return bSuspendDetailsPanelRefresh; }
	TArray<TWeakObjectPtr<UObject>> GetSelectedObjects() const;
	virtual void SetDetailObjects(const TArray<UObject*>& InObjects);
	virtual void SetDetailObjects(const TArray<UObject*>& InObjects, bool bChangeUISelectionState);
	virtual void SetDetailObjectFilter(TSharedPtr<FDetailsViewObjectFilter> InObjectFilter);
	void SetMemoryStorageDetails(const TArray<FRigVMMemoryStorageStruct*>& InStructs);
	void SetDetailViewForGraph(URigVMGraph* InGraph);
	void SetDetailViewForFocusedGraph();
	void SetDetailViewForLocalVariable();
	virtual void RefreshDetailView();
	bool DetailViewShowsAnyRigUnit() const;
	bool DetailViewShowsLocalVariable() const;
	bool DetailViewShowsStruct(UScriptStruct* InStruct) const;
	void ClearDetailObject(bool bChangeUISelectionState = true);
	void ClearDetailsViewWrapperObjects();
	const TArray<TStrongObjectPtr<URigVMDetailsViewWrapperObject>>& GetWrapperObjects() const { return WrapperObjects; }

	void SetHost(URigVMHost* InHost);

	virtual URigVMGraph* GetFocusedModel() const override;
	URigVMController* GetFocusedController() const;
	TSharedPtr<SGraphEditor> GetGraphEditor(UEdGraph* InEdGraph) const;

	/** Extend menu */
	void ExtendMenu();

	/** Extend toolbar */
	void ExtendToolbar();
	
	/** Fill the toolbar with content */
	virtual void FillToolbar(FToolBarBuilder& ToolbarBuilder, bool bEndSection = true);
	
	virtual TArray<FName> GetDefaultEventQueue() const;
	TArray<FName> GetEventQueue() const;
	void SetEventQueue(TArray<FName> InEventQueue);
	virtual void SetEventQueue(TArray<FName> InEventQueue, bool bCompile);
	virtual int32 GetEventQueueComboValue() const { return INDEX_NONE; }
	virtual FText GetEventQueueLabel() const { return FText(); }
	virtual FSlateIcon GetEventQueueIcon(const TArray<FName>& InEventQueue) const;
	FSlateIcon GetEventQueueIcon() const;

	void SetExecutionMode(const ERigVMEditorExecutionModeType InExecutionMode);
	int32 GetExecutionModeComboValue() const;
	FText GetExecutionModeLabel() const;
	static FSlateIcon GetExecutionModeIcon(const ERigVMEditorExecutionModeType InExecutionMode);
	FSlateIcon GetExecutionModeIcon() const;
	
	virtual void GetDebugObjects(TArray<FRigVMCustomDebugObject>& DebugList) const override;
	virtual bool OnlyShowCustomDebugObjects() const override { return true; }
	void HandleSetObjectBeingDebugged(UObject* InObject);
	virtual FString GetCustomDebugObjectLabel(UObject* ObjectBeingDebugged) const override;

	/** Handle hiding items in the graph */
	void HandleHideItem();
	bool CanHideItem() const;

	/** Update stale watch pins */
	void UpdateStaleWatchedPins();
	
	virtual void HandleRefreshEditorFromBlueprint(URigVMBlueprint* InBlueprint);
	void HandleVariableDroppedFromBlueprint(UObject* InSubject, FProperty* InVariableToDrop, const FVector2D& InDropPosition, const FVector2D& InScreenPosition);
	void HandleBreakpointAdded();
	void OnGraphNodeClicked(URigVMEdGraphNode* InNode);
	void OnNodeDoubleClicked(URigVMBlueprint* InBlueprint, URigVMNode* InNode);
	void OnGraphImported(UEdGraph* InEdGraph);
	bool OnActionMatchesName(FEdGraphSchemaAction* InAction, const FName& InName) const;
	void HandleShowCurrentStatement();
	void HandleBreakpointActionRequested(const ERigVMBreakpointAction BreakpointAction);
	bool IsHaltedAtBreakpoint() const;
	void FrameSelection();
	void SwapFunctionWithinAsset();
	void SwapFunctionAcrossProject();
	void SwapFunctionForAssets(const TArray<FAssetData>& InAssets, bool bSetupUndo);
	void SwapAssetReferences();


	/** Once the log is collected update the graph */
	void UpdateGraphCompilerErrors();

	/** Returns true if PIE is currently running */
	static bool IsPIERunning();

	void OnPIEStopped(bool bSimulation);

	/** Our currently running rig vm instance */
	//TObjectPtr<URigVMHost> Host;

	FPreviewHostUpdated PreviewHostUpdated;

	/** Toolbox hosting widget */
	TSharedPtr<SBorder> Toolbox;

	TSharedPtr<SRigVMEditorGraphExplorer> GraphExplorerWidget;

	FRigVMEditorClosed RigVMEditorClosedDelegate;

	virtual void SetGraphExplorerWidget(TSharedPtr<SRigVMEditorGraphExplorer> InWidget) override { GraphExplorerWidget = InWidget; }
	virtual TSharedPtr<SRigVMEditorGraphExplorer> GetGraphExplorerWidget() override { return GraphExplorerWidget; }

	bool IsEditingSingleBlueprint() const;
	
protected:
	bool bAnyErrorsLeft;
	TMap<FString, FString> KnownInstructionLimitWarnings;
	URigVMNode* HaltedAtNode;
	FString LastDebuggedHost;

	bool bSuspendDetailsPanelRefresh;
	bool bDetailsPanelRequiresClear;
	bool bAllowBulkEdits;
	bool bIsSettingObjectBeingDebugged;

	bool bRigVMEditorInitialized;

	/** Are we currently compiling through the user interface */
	bool bIsCompilingThroughUI;

	TArray<TStrongObjectPtr<URigVMDetailsViewWrapperObject>> WrapperObjects;

	ERigVMEditorExecutionModeType ExecutionMode;

	/** The log to use for errors resulting from the init phase of the units */
	FRigVMLog RigVMLog;
	
	TArray<FName> LastEventQueue;

	/** The extender to pass to the level editor to extend it's window menu */
	TSharedPtr<FExtender> MenuExtender;

	/** Toolbar extender */
	TSharedPtr<FExtender> ToolbarExtender;

	FDelegateHandle PropertyChangedHandle;

	FOnRefreshEvent OnRefreshEvent;

	friend class SRigVMExecutionStackView;
	friend class SRigVMEditorGraphExplorer;
};

struct FRigVMEditorZoomLevelsContainer : public FZoomLevelsContainer
{
	struct FRigVMEditorZoomLevelEntry
	{
	public:
		FRigVMEditorZoomLevelEntry(float InZoomAmount, const FText& InDisplayText, EGraphRenderingLOD::Type InLOD)
			: DisplayText(FText::Format(NSLOCTEXT("GraphEditor", "Zoom", "Zoom {0}"), InDisplayText))
		, ZoomAmount(InZoomAmount)
		, LOD(InLOD)
		{
		}

	public:
		FText DisplayText;
		float ZoomAmount;
		EGraphRenderingLOD::Type LOD;
	};
	
	FRigVMEditorZoomLevelsContainer()
	{
		ZoomLevels.Reserve(22);
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.025f, FText::FromString(TEXT("-14")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.070f, FText::FromString(TEXT("-13")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.100f, FText::FromString(TEXT("-12")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.125f, FText::FromString(TEXT("-11")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.150f, FText::FromString(TEXT("-10")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.175f, FText::FromString(TEXT("-9")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.200f, FText::FromString(TEXT("-8")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.225f, FText::FromString(TEXT("-7")), EGraphRenderingLOD::LowDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.250f, FText::FromString(TEXT("-6")), EGraphRenderingLOD::LowDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.375f, FText::FromString(TEXT("-5")), EGraphRenderingLOD::MediumDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.500f, FText::FromString(TEXT("-4")), EGraphRenderingLOD::MediumDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.675f, FText::FromString(TEXT("-3")), EGraphRenderingLOD::MediumDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.750f, FText::FromString(TEXT("-2")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(0.875f, FText::FromString(TEXT("-1")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(1.000f, FText::FromString(TEXT("1:1")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(1.250f, FText::FromString(TEXT("+1")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(1.375f, FText::FromString(TEXT("+2")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(1.500f, FText::FromString(TEXT("+3")), EGraphRenderingLOD::FullyZoomedIn));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(1.675f, FText::FromString(TEXT("+4")), EGraphRenderingLOD::FullyZoomedIn));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(1.750f, FText::FromString(TEXT("+5")), EGraphRenderingLOD::FullyZoomedIn));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(1.875f, FText::FromString(TEXT("+6")), EGraphRenderingLOD::FullyZoomedIn));
		ZoomLevels.Add(FRigVMEditorZoomLevelEntry(2.000f, FText::FromString(TEXT("+7")), EGraphRenderingLOD::FullyZoomedIn));
	}

	float GetZoomAmount(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].ZoomAmount;
	}

	int32 GetNearestZoomLevel(float InZoomAmount) const override
	{
		for (int32 ZoomLevelIndex=0; ZoomLevelIndex < GetNumZoomLevels(); ++ZoomLevelIndex)
		{
			if (InZoomAmount <= GetZoomAmount(ZoomLevelIndex))
			{
				return ZoomLevelIndex;
			}
		}

		return GetDefaultZoomLevel();
	}
	
	FText GetZoomText(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].DisplayText;
	}
	
	int32 GetNumZoomLevels() const override
	{
		return ZoomLevels.Num();
	}
	
	int32 GetDefaultZoomLevel() const override
	{
		return 14;
	}

	EGraphRenderingLOD::Type GetLOD(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].LOD;
	}

	TArray<FRigVMEditorZoomLevelEntry> ZoomLevels;
};

#undef LOCTEXT_NAMESPACE