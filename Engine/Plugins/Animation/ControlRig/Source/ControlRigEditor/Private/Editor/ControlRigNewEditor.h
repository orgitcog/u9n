// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRigEditor.h"
#include "Editor/SRigVMDetailsInspector.h"
#include "PreviewScene.h"

class FControlRigEditor : public IControlRigNewEditor, public FControlRigBaseEditor
{
public:

	virtual void InitRigVMEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class URigVMBlueprint* InRigVMBlueprint) override { return FControlRigBaseEditor::InitRigVMEditorImpl(Mode, InitToolkitHost, InRigVMBlueprint); }
	virtual void InitRigVMEditorSuper(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, class URigVMBlueprint* InRigVMBlueprint) override { return IControlRigNewEditor::InitRigVMEditor(Mode, InitToolkitHost, InRigVMBlueprint); }

	virtual const FName GetEditorAppName() const override { return FControlRigBaseEditor::GetEditorAppNameImpl(); }
	virtual const FName GetEditorModeName() const override { return FControlRigBaseEditor::GetEditorModeNameImpl(); }
	virtual TSharedPtr<FApplicationMode> CreateEditorMode() override;
	virtual const FSlateBrush* GetDefaultTabIcon() const override { return FControlRigBaseEditor::GetDefaultTabIconImpl(); }
	
public:
	FControlRigEditor();
	virtual ~FControlRigEditor();

	// FControlRigBaseEditor
	virtual bool IsControlRigLegacyEditor() const override { return false; }
	virtual TSharedPtr<FAssetEditorToolkit> GetHostingApp() override { return IControlRigNewEditor::GetHostingApp(); }
	virtual TSharedRef<IControlRigBaseEditor> SharedControlRigEditorRef() override { return StaticCastSharedRef<IControlRigBaseEditor>(SharedThis(this)); }
	virtual TSharedRef<IRigVMEditor> SharedRigVMEditorRef() override { return StaticCastSharedRef<IRigVMEditor>(SharedThis(this)); }
	virtual TSharedRef<const IRigVMEditor> SharedRigVMEditorRef() const override { return StaticCastSharedRef<const IRigVMEditor>(SharedThis(this)); }
	
	virtual bool IsControlRigNewEditor() const { return true; };
	virtual URigVMBlueprint* GetRigVMBlueprint() const { return IControlRigNewEditor::GetRigVMBlueprint(); }
	virtual URigVMHost* GetRigVMHost() const { return FRigVMEditorBase::GetRigVMHost(); }
	virtual TSharedRef<FUICommandList> GetToolkitCommands() { return IControlRigNewEditor::GetToolkitCommands(); }
	virtual FPreviewScene* GetPreviewScene() { return &PreviewScene; }
	virtual bool IsDetailsPanelRefreshSuspended() const { return IControlRigNewEditor::IsDetailsPanelRefreshSuspended(); }
	virtual TArray<TWeakObjectPtr<UObject>> GetSelectedObjects() const { return IControlRigNewEditor::GetSelectedObjects(); }
	virtual void ClearDetailObject(bool bChangeUISelectionState = true) { IControlRigNewEditor::ClearDetailObject(bChangeUISelectionState); }
	virtual bool DetailViewShowsStruct(UScriptStruct* InStruct) const { return IControlRigNewEditor::DetailViewShowsStruct(InStruct); }
	virtual TSharedPtr<class SWidget> GetInspector() const { return Inspector; }
	virtual TArray<FName> GetEventQueue() const { return IControlRigNewEditor::GetEventQueue(); }
	virtual void SummonSearchUI(bool bSetFindWithinBlueprint, FString NewSearchTerms = FString(), bool bSelectFirstResult = false) { IControlRigNewEditor::SummonSearchUI(bSetFindWithinBlueprint, NewSearchTerms, bSelectFirstResult); }
	virtual const TArray< UObject* >* GetObjectsCurrentlyBeingEdited() const { return IControlRigNewEditor::GetObjectsCurrentlyBeingEdited(); }
	virtual FEditorModeTools& GetEditorModeManagerImpl() const { return GetEditorModeManager(); }
	virtual const FName GetEditorModeNameImpl() const { return GetEditorModeName(); }
	virtual URigVMController* GetFocusedController() const { return IControlRigNewEditor::GetFocusedController(); }
	virtual URigVMGraph* GetFocusedModel() const { return IControlRigNewEditor::GetFocusedModel(); }
	virtual TArray<FName> GetLastEventQueue() const override { return LastEventQueue; }


	// FRigVMEditorBase interface
	virtual UObject* GetOuterForHost() const override { return GetOuterForHostImpl();}
	virtual UObject* GetOuterForHostSuper() const override { return IControlRigNewEditor::GetOuterForHost(); }
	
	virtual UClass* GetDetailWrapperClass() const { return GetDetailWrapperClassImpl(); }
	virtual void Compile() override { CompileBaseImpl(); }
	virtual void CompileSuper() override { IControlRigNewEditor::Compile(); }

	virtual void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject) override { FControlRigBaseEditor::HandleModifiedEventImpl(InNotifType, InGraph, InSubject); }
	virtual void HandleModifiedEventSuper(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject) override { IControlRigNewEditor::HandleModifiedEvent(InNotifType, InGraph, InSubject); }

	virtual void OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList) override { FControlRigBaseEditor::OnCreateGraphEditorCommandsImpl(GraphEditorCommandsList); }
	virtual void OnCreateGraphEditorCommandsSuper(TSharedPtr<FUICommandList> GraphEditorCommandsList) override { IControlRigNewEditor::OnCreateGraphEditorCommands(GraphEditorCommandsList); }
	
	virtual void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM, FRigVMExtendedExecuteContext& InContext) override { FControlRigBaseEditor::HandleVMCompiledEventImpl(InCompiledObject, InVM, InContext); }
	virtual void HandleVMCompiledEventSuper(UObject* InCompiledObject, URigVM* InVM, FRigVMExtendedExecuteContext& InContext) override { IControlRigNewEditor::HandleVMCompiledEvent(InCompiledObject, InVM, InContext); }
	
	virtual bool ShouldOpenGraphByDefault() const override { return FControlRigBaseEditor::ShouldOpenGraphByDefaultImpl(); }
	virtual FReply OnViewportDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override { return FControlRigBaseEditor::OnViewportDropImpl(MyGeometry, DragDropEvent); }
	virtual FReply OnViewportDropSuper(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override { return IControlRigNewEditor::OnViewportDrop(MyGeometry, DragDropEvent); }

	// allows the editor to fill an empty graph
	virtual void CreateEmptyGraphContent(URigVMController* InController) override { FControlRigBaseEditor::CreateEmptyGraphContentImpl(InController); }

public:
	
	// IToolkit Interface
	virtual FName GetToolkitFName() const override { return FControlRigBaseEditor::GetToolkitFNameImpl(); }
	virtual FText GetBaseToolkitName() const override { return FControlRigBaseEditor::GetBaseToolkitNameImpl(); }
	virtual FString GetWorldCentricTabPrefix() const override { return FControlRigBaseEditor::GetWorldCentricTabPrefixImpl(); }
	virtual FString GetDocumentationLink() const override { return FControlRigBaseEditor::GetDocumentationLinkImpl(); }

	// BlueprintEditor interface
	virtual FReply OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2f& InPosition, UEdGraph* InGraph) override { return FControlRigBaseEditor::OnSpawnGraphNodeByShortcutImpl(InChord, InPosition, InGraph); }
	virtual FReply OnSpawnGraphNodeByShortcutSuper(FInputChord InChord, const FVector2f& InPosition, UEdGraph* InGraph) override { return IControlRigNewEditor::OnSpawnGraphNodeByShortcut(InChord, InPosition, InGraph); }

	virtual bool IsSectionVisible(RigVMNodeSectionID::Type InSectionID) const override;
	virtual bool NewDocument_IsVisibleForType(FRigVMEditorBase::ECreatedDocumentType GraphType) const override;


	virtual void PostUndo(bool bSuccess) override;
	virtual void PostTransaction(bool bSuccess, const FTransaction* Transaction, bool bIsRedo) override;

	//  FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual void TickSuper(float DeltaTime) override { return IControlRigNewEditor::Tick(DeltaTime); }

	virtual void SetDetailObjects(const TArray<UObject*>& InObjects) override { return FControlRigBaseEditor::SetDetailObjectsImpl(InObjects); }
	virtual void SetDetailObjectsSuper(const TArray<UObject*>& InObjects) override { return IControlRigNewEditor::SetDetailObjects(InObjects); }
	virtual void SetDetailObjectFilter(TSharedPtr<FDetailsViewObjectFilter> InObjectFilter) override { return IControlRigNewEditor::SetDetailObjectFilter(InObjectFilter); };
	virtual void RefreshDetailView() override { return FControlRigBaseEditor::RefreshDetailViewImpl(); }
	virtual void RefreshDetailViewSuper() override { return IControlRigNewEditor::RefreshDetailView(); }

public:

	
	virtual void OnGraphNodeDropToPerform(TSharedPtr<FDragDropOperation> InDragDropOp, UEdGraph* InGraph, const FVector2f& InNodePosition, const FVector2f& InScreenPosition) override  { return FControlRigBaseEditor::OnGraphNodeDropToPerformImpl(InDragDropOp, InGraph, InNodePosition, InScreenPosition); }
	virtual void OnGraphNodeDropToPerformSuper(TSharedPtr<FDragDropOperation> InDragDropOp, UEdGraph* InGraph, const FVector2f& InNodePosition, const FVector2f& InScreenPosition) override  { return IControlRigNewEditor::OnGraphNodeDropToPerform(InDragDropOp, InGraph, InNodePosition, InScreenPosition); }

	
protected:

	virtual void BindCommands() override { return FControlRigBaseEditor::BindCommandsImpl(); }
	virtual void BindCommandsSuper() override { return IControlRigNewEditor::BindCommands(); }
	virtual FMenuBuilder GenerateBulkEditMenu() override { return FControlRigBaseEditor::GenerateBulkEditMenuImpl(); }
	virtual FMenuBuilder GenerateBulkEditMenuSuper() override { return IControlRigNewEditor::GenerateBulkEditMenu(); }

	virtual void SaveAsset_Execute() override { return FControlRigBaseEditor::SaveAsset_ExecuteImpl(); }
	virtual void SaveAsset_ExecuteSuper() override { return FRigVMEditorBase::SaveAsset_Execute(); }
	virtual void SaveAssetAs_Execute() override { return FControlRigBaseEditor::SaveAssetAs_ExecuteImpl(); }
	virtual void SaveAssetAs_ExecuteSuper() override { return FRigVMEditorBase::SaveAssetAs_Execute(); }

	virtual void HandleVMExecutedEvent(URigVMHost* InHost, const FName& InEventName) override { return FControlRigBaseEditor::HandleVMExecutedEventImpl(InHost, InEventName); }
	virtual void HandleVMExecutedEventSuper(URigVMHost* InHost, const FName& InEventName) override { return IControlRigNewEditor::HandleVMExecutedEvent(InHost, InEventName); }

	// FBaseToolKit overrides
	virtual void CreateEditorModeManager() override { return FControlRigBaseEditor::CreateEditorModeManagerImpl(); }

private:
	/** Fill the toolbar with content */
	virtual void FillToolbar(FToolBarBuilder& ToolbarBuilder, bool bEndSection = true) override { return FControlRigBaseEditor::FillToolbarImpl(ToolbarBuilder, bEndSection); }
	virtual void FillToolbarSuper(FToolBarBuilder& ToolbarBuilder, bool bEndSection = true) override { return IControlRigNewEditor::FillToolbar(ToolbarBuilder, bEndSection); }

	virtual TArray<FName> GetDefaultEventQueue() const override { return FControlRigBaseEditor::GetDefaultEventQueueImpl(); }
	virtual void SetEventQueue(TArray<FName> InEventQueue, bool bCompile) override { return FControlRigBaseEditor::SetEventQueueImpl(InEventQueue, bCompile); }
	virtual void SetEventQueueSuper(TArray<FName> InEventQueue, bool bCompile) override { return IControlRigNewEditor::SetEventQueue(InEventQueue, bCompile); }
	virtual void SetEventQueueSuper(TArray<FName> InEventQueue) override { return IControlRigNewEditor::SetEventQueue(InEventQueue); }
	virtual int32 GetEventQueueComboValue() const override { return FControlRigBaseEditor::GetEventQueueComboValueImpl(); }
	virtual int32 GetEventQueueComboValueSuper() const override { return IControlRigNewEditor::GetEventQueueComboValue(); }
	virtual FText GetEventQueueLabel() const override { return FControlRigBaseEditor::GetEventQueueLabelImpl(); }
	virtual FSlateIcon GetEventQueueIcon(const TArray<FName>& InEventQueue) const override { return FControlRigBaseEditor::GetEventQueueIconImpl(InEventQueue); }
	virtual void HandleSetObjectBeingDebugged(UObject* InObject) override { return FControlRigBaseEditor::HandleSetObjectBeingDebuggedImpl(InObject); }
	virtual void HandleSetObjectBeingDebuggedSuper(UObject* InObject) override { return IControlRigNewEditor::HandleSetObjectBeingDebugged(InObject); }

	/** Push a newly compiled/opened control rig to the edit mode */
	virtual void UpdateRigVMHost() override { return FControlRigBaseEditor::UpdateRigVMHostImpl(); }
	virtual void UpdateRigVMHostSuper() override { return IControlRigNewEditor::UpdateRigVMHost(); }
	virtual void UpdateRigVMHost_PreClearOldHost(URigVMHost* InPreviousHost) override { return FControlRigBaseEditor::UpdateRigVMHost_PreClearOldHostImpl(InPreviousHost); }

	/** Update the name lists for use in name combo boxes */
	virtual void CacheNameLists() override { return FControlRigBaseEditor::CacheNameListsImpl(); }
	virtual void CacheNameListsSuper() override { return IControlRigNewEditor::CacheNameLists(); }

	virtual void GenerateEventQueueMenuContent(FMenuBuilder& MenuBuilder) override { return FControlRigBaseEditor::GenerateEventQueueMenuContentImpl(MenuBuilder); }

	virtual void HandleRefreshEditorFromBlueprint(URigVMBlueprint* InBlueprint) override { return FControlRigBaseEditor::HandleRefreshEditorFromBlueprintImpl(InBlueprint); }
	virtual void HandleRefreshEditorFromBlueprintSuper(URigVMBlueprint* InBlueprint) override { return IControlRigNewEditor::HandleRefreshEditorFromBlueprint(InBlueprint); }

	
	/** delegate for changing property */
	virtual void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) override { return FControlRigBaseEditor::OnFinishedChangingPropertiesImpl(PropertyChangedEvent); }
	virtual void OnFinishedChangingPropertiesSuper(const FPropertyChangedEvent& PropertyChangedEvent) override { return IControlRigNewEditor::OnFinishedChangingProperties(PropertyChangedEvent); }
	virtual void OnWrappedPropertyChangedChainEvent(URigVMDetailsViewWrapperObject* InWrapperObject, const FString& InPropertyPath, FPropertyChangedChainEvent& InPropertyChangedChainEvent) override { return FControlRigBaseEditor::OnWrappedPropertyChangedChainEventImpl(InWrapperObject, InPropertyPath, InPropertyChangedChainEvent); }
	virtual void OnWrappedPropertyChangedChainEventSuper(URigVMDetailsViewWrapperObject* InWrapperObject, const FString& InPropertyPath, FPropertyChangedChainEvent& InPropertyChangedChainEvent) override { return IControlRigNewEditor::OnWrappedPropertyChangedChainEvent(InWrapperObject, InPropertyPath, InPropertyChangedChainEvent); }

	virtual void SetEditorModeManager(TSharedPtr<FEditorModeTools> InManager) override { EditorModeManager = InManager; }

	virtual const TArray<TStrongObjectPtr<URigVMDetailsViewWrapperObject>>& GetWrapperObjects() const override { return IControlRigNewEditor::GetWrapperObjects(); }
	virtual bool& GetSuspendDetailsPanelRefreshFlag() override { return IControlRigNewEditor::GetSuspendDetailsPanelRefreshFlag(); }

	virtual TWeakPtr<class SGraphEditor> GetFocusedGraphEd() const override { return FocusedGraphEdPtr;}

	virtual void OnClose() override { FControlRigBaseEditor::OnClose(); }
	virtual void OnCloseSuper() override { IControlRigNewEditor::OnClose(); }

private:
	/** Blueprint preview scene */
	FPreviewScene PreviewScene;
};
