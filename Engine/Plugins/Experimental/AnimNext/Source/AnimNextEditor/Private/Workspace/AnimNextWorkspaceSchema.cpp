// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextWorkspaceSchema.h"

#include "AnimNextEditorModule.h"
#include "AnimNextWorkspaceEditorMode.h"
#include "AssetEditorModeManager.h"
#include "IWorkspaceEditor.h"
#include "StructUtils/InstancedStruct.h"

#define LOCTEXT_NAMESPACE "AnimNextWorkspaceSchema"

FText UAnimNextWorkspaceSchema::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "UAF Workspace");
}

TConstArrayView<FTopLevelAssetPath> UAnimNextWorkspaceSchema::GetSupportedAssetClassPaths() const
{
	const UE::AnimNext::Editor::FAnimNextEditorModule& Module = FModuleManager::Get().LoadModuleChecked<UE::AnimNext::Editor::FAnimNextEditorModule>("AnimNextEditor");

	return Module.SupportedAssetClasses;
}

void UAnimNextWorkspaceSchema::OnSaveWorkspaceState(TSharedRef<UE::Workspace::IWorkspaceEditor> InWorkspaceEditor, FInstancedStruct& OutWorkspaceState) const
{
	UAnimNextWorkspaceEditorMode* EditorMode = Cast<UAnimNextWorkspaceEditorMode>(InWorkspaceEditor->GetEditorModeManager().GetActiveScriptableMode(UAnimNextWorkspaceEditorMode::EM_AnimNextWorkspace));
	if(EditorMode == nullptr)
	{
		return;
	}

	OutWorkspaceState = FInstancedStruct::Make(EditorMode->State);
}

void UAnimNextWorkspaceSchema::OnLoadWorkspaceState(TSharedRef< UE::Workspace::IWorkspaceEditor> InWorkspaceEditor, const FInstancedStruct& InWorkspaceState) const
{
	// Activate and set up our mode
	InWorkspaceEditor->GetEditorModeManager().ActivateMode(UAnimNextWorkspaceEditorMode::EM_AnimNextWorkspace);

	UAnimNextWorkspaceEditorMode* EditorMode = Cast<UAnimNextWorkspaceEditorMode>(InWorkspaceEditor->GetEditorModeManager().GetActiveScriptableMode(UAnimNextWorkspaceEditorMode::EM_AnimNextWorkspace));
	if(EditorMode == nullptr)
	{
		return;
	}

	const FAnimNextWorkspaceState* State = InWorkspaceState.GetPtr<FAnimNextWorkspaceState>();
	if(State == nullptr)
	{
		return;
	}

	EditorMode->State = *State;
	EditorMode->PropagateAutoCompile(InWorkspaceEditor, EditorMode->State.bAutoCompile);
}

#undef LOCTEXT_NAMESPACE