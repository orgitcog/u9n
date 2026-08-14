// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetCompilationHandler.h"
#include "AnimNextRigVMAsset.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "AnimNextScopedCompilerResults.h"
#include "UncookedOnlyUtils.h"

namespace UE::AnimNext::Editor
{

FAssetCompilationHandler::FAssetCompilationHandler(UObject* InAsset)
{
	UAnimNextRigVMAsset* AnimNextRigVMAsset = CastChecked<UAnimNextRigVMAsset>(InAsset);
	UAnimNextRigVMAssetEditorData* EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(AnimNextRigVMAsset);

	NumErrors = EditorData->bErrorsDuringCompilation ? 1 : 0;
	NumWarnings = EditorData->bWarningsDuringCompilation ? 1 : 0;
}

void FAssetCompilationHandler::Compile(TSharedRef<Workspace::IWorkspaceEditor> InWorkspaceEditor, UObject* InAsset)
{
	UAnimNextRigVMAsset* AnimNextRigVMAsset = CastChecked<UAnimNextRigVMAsset>(InAsset);
	UAnimNextRigVMAssetEditorData* EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(AnimNextRigVMAsset);

	UncookedOnly::FScopedCompilerResults CompilerResults(InAsset);
	EditorData->RecompileVM();

	NumErrors = CompilerResults.GetLog().NumErrors;
	NumWarnings = CompilerResults.GetLog().NumWarnings;

	OnCompileStatusChanged().ExecuteIfBound();
}

void FAssetCompilationHandler::SetAutoCompile(TSharedRef<Workspace::IWorkspaceEditor> InWorkspaceEditor, UObject* InAsset, bool bInAutoCompile)
{
	UAnimNextRigVMAsset* AnimNextRigVMAsset = CastChecked<UAnimNextRigVMAsset>(InAsset);
	UAnimNextRigVMAssetEditorData* EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(AnimNextRigVMAsset);
	EditorData->SetAutoVMRecompile(bInAutoCompile);
}

bool FAssetCompilationHandler::GetAutoCompile(TSharedRef<Workspace::IWorkspaceEditor> InWorkspaceEditor, const UObject* InAsset) const
{
	const UAnimNextRigVMAsset* AnimNextRigVMAsset = CastChecked<UAnimNextRigVMAsset>(InAsset);
	UAnimNextRigVMAssetEditorData* EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(AnimNextRigVMAsset);
	return EditorData->GetAutoVMRecompile();
}

ECompileStatus FAssetCompilationHandler::GetCompileStatus(TSharedRef<Workspace::IWorkspaceEditor> InWorkspaceEditor, const UObject* InAsset) const
{
	const UAnimNextRigVMAsset* AnimNextRigVMAsset = CastChecked<UAnimNextRigVMAsset>(InAsset);
	UAnimNextRigVMAssetEditorData* EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(AnimNextRigVMAsset);
	if(EditorData->bErrorsDuringCompilation || NumErrors > 0)
	{
		return ECompileStatus::Error;
	}

	if(EditorData->bWarningsDuringCompilation || NumWarnings > 0)
	{
		return ECompileStatus::Warning;
	}

	if(EditorData->IsDirtyForRecompilation())
	{
		return ECompileStatus::Dirty;
	}

	return ECompileStatus::UpToDate;
}

}
