// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_RIGVMLEGACYEDITOR
#include "Editor/RigVMLegacyEditor.h"
#include "SBlueprintEditorToolbar.h"
#include "SGraphPanel.h"
#include "SMyBlueprint.h"
#include "RigVMEditorModule.h"
#include "SBlueprintEditorSelectedDebugObjectWidget.h"
#include "RigVMEditorCommands.h"

struct FRigVMEditorZoomLevelsContainer;

FRigVMLegacyEditor::FRigVMLegacyEditor()
{
	
}

void FRigVMLegacyEditor::OnClose()
{
	FRigVMEditorBase::UnbindEditor();
	return FBlueprintEditor::OnClose();
}

void FRigVMLegacyEditor::InitAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const FName AppIdentifier,
                                         const TSharedRef<FTabManager::FLayout>& StandaloneDefaultLayout, const bool bCreateDefaultStandaloneMenu, const bool bCreateDefaultToolbar,
                                         const TArray<UObject*>& ObjectsToEdit, const bool bInIsToolbarFocusable, const bool bInUseSmallToolbarIcons,
                                         const TOptional<EAssetOpenMethod>& InOpenMethod)
{
	FBlueprintEditor::InitAssetEditor(Mode, InitToolkitHost, AppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit, bInIsToolbarFocusable, bInUseSmallToolbarIcons, InOpenMethod);
}

void FRigVMLegacyEditor::CreateEditorToolbar()
{
	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(FBlueprintEditor::SharedThis<FBlueprintEditor>(this)));
	}
}

TSharedPtr<FApplicationMode> FRigVMLegacyEditor::CreateEditorMode()
{
	return MakeShareable(new FRigVMLegacyEditorMode(StaticCastSharedRef<FRigVMLegacyEditor>(FBlueprintEditor::SharedThis<FBlueprintEditor>(this))));
}

const FName FRigVMLegacyEditor::GetEditorAppName() const
{
	static const FLazyName AppName(TEXT("RigVMLegacyEditorApp"));
	return AppName;
}

void FRigVMLegacyEditor::OnGraphEditorFocused(const TSharedRef<SGraphEditor>& InGraphEditor)
{
	FBlueprintEditor::OnGraphEditorFocused(InGraphEditor);
	FRigVMEditorBase::OnGraphEditorFocused(InGraphEditor);
}

void FRigVMLegacyEditor::AddCompileWidget(FToolBarBuilder& ToolbarBuilder)
{
	if (UToolMenu* ToolMenu = RegisterModeToolbarIfUnregistered(FRigVMEditorModes::RigVMEditorMode))
	{
		GetToolbarBuilder()->AddCompileToolbar(ToolMenu);
	}
}

void FRigVMLegacyEditor::AddSelectedDebugObjectWidget(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddWidget(SNew(SBlueprintEditorSelectedDebugObjectWidget, SharedThis(this)));
}

void FRigVMLegacyEditor::AddAutoCompileWidget(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddToolBarButton(FRigVMEditorCommands::Get().AutoCompileGraph,
		NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FRigVMEditorStyle::Get().GetStyleSetName(), "RigVM.AutoCompileGraph"));
}

void FRigVMLegacyEditor::Tick(float DeltaTime)
{
	FBlueprintEditor::Tick(DeltaTime);
	FRigVMEditorBase::Tick(DeltaTime);
}

void FRigVMLegacyEditor::GetCustomDebugObjects(TArray<FCustomDebugObject>& DebugList) const
{
	TArray<FRigVMCustomDebugObject> RigVMList;
	RigVMList.Reserve(DebugList.Num());
	for (FCustomDebugObject Obj : DebugList)
	{
		FRigVMCustomDebugObject RigVMObj;
		RigVMObj.Object = Obj.Object;
		RigVMObj.NameOverride = Obj.NameOverride;
		RigVMList.Add(RigVMObj);
	}

	GetDebugObjects(RigVMList);

	DebugList.Reset();
	DebugList.Reserve(RigVMList.Num());
	for (FRigVMCustomDebugObject RigVMObj : RigVMList)
	{
		FCustomDebugObject Obj;
		Obj.Object = RigVMObj.Object;
		Obj.NameOverride = RigVMObj.NameOverride;
		DebugList.Add(Obj);
	}
}

void FRigVMLegacyEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	FBlueprintEditor::InitToolMenuContext(MenuContext);
	FRigVMEditorBase::InitToolMenuContextImpl(MenuContext);
}

bool FRigVMLegacyEditor::TransactionObjectAffectsBlueprint(UObject* InTransactedObject)
{
	return FRigVMEditorBase::TransactionObjectAffectsBlueprintImpl(InTransactedObject)
	&& FBlueprintEditor::TransactionObjectAffectsBlueprint(InTransactedObject);
}

FEdGraphPinType FRigVMLegacyEditor::GetLastPinTypeUsed()
{
	return MyBlueprintWidget->GetLastPinTypeUsed();
}

void FRigVMLegacyEditor::JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename)
{
	if (FRigVMEditorBase::JumpToHyperlinkImpl(ObjectReference, bRequestRename))
	{
		return;
	}
	FBlueprintEditor::JumpToHyperlink(ObjectReference, bRequestRename);
}

void FRigVMLegacyEditor::PostUndo(bool bSuccess)
{
	FBlueprintEditor::PostUndo(bSuccess);
	FRigVMEditorBase::PostUndoImpl(bSuccess);
}

void FRigVMLegacyEditor::PostRedo(bool bSuccess)
{
	FBlueprintEditor::PostRedo(bSuccess);
	FRigVMEditorBase::PostUndoImpl(bSuccess);
}

void FRigVMLegacyEditor::CreateDefaultCommands()
{
	if (FBlueprintEditor::GetBlueprintObj())
	{
		FBlueprintEditor::CreateDefaultCommands();
	}

	FRigVMEditorBase::CreateDefaultCommandsImpl();
}

TSharedRef<SGraphEditor> FRigVMLegacyEditor::CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph)
{
	TSharedRef<SGraphEditor> GraphEditor = FBlueprintEditor::CreateGraphEditorWidget(InTabInfo, InGraph);
	GraphEditor->GetGraphPanel()->SetZoomLevelsContainer<FRigVMEditorZoomLevelsContainer>();
	return GraphEditor;
}

void FRigVMLegacyEditor::RefreshEditorsImpl(ERefreshRigVMEditorReason::Type Reason)
{
	FBlueprintEditor::RefreshEditors((ERefreshBlueprintEditorReason::Type)Reason);
}

bool FRigVMLegacyEditor::NewDocument_IsVisibleForType(FBlueprintEditor::ECreatedDocumentType GraphType) const
{
	if (FRigVMEditorBase::NewDocument_IsVisibleForTypeImpl((FRigVMEditorBase::ECreatedDocumentType)GraphType))
	{
		return FBlueprintEditor::NewDocument_IsVisibleForType(GraphType);
	}
	return false;
}

bool FRigVMLegacyEditor::IsSectionVisible(NodeSectionID::Type InSectionID) const
{
	return FRigVMEditorBase::IsSectionVisibleImpl((RigVMNodeSectionID::Type)InSectionID);
}


FName FRigVMLegacyEditor::GetSelectedVariableName()
{
	FName VariableName;
	if (MyBlueprintWidget.IsValid())
	{
		if (FEdGraphSchemaAction_BlueprintVariableBase* VariableAcion = MyBlueprintWidget->SelectionAsBlueprintVariable())
		{
			return VariableAcion->GetVariableName();
		}
	}
	return VariableName;
}

#endif
