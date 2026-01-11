// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnimationGraphAssetDefinition.h"

#include "AnimNextRigVMAssetEditorData.h"
#include "ContentBrowserMenuContexts.h"
#include "FileHelpers.h"
#include "IWorkspaceEditorModule.h"
#include "ToolMenus.h"
#include "UncookedOnlyUtils.h"
#include "Workspace/AnimNextWorkspaceFactory.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"

EAssetCommandResult UAssetDefinition_AnimNextAnimationGraph::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace UE::AnimNext::Editor;
	using namespace UE::Workspace;

	for (UAnimNextAnimationGraph* Asset : OpenArgs.LoadObjects<UAnimNextAnimationGraph>())
	{
		IWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::Get().LoadModuleChecked<IWorkspaceEditorModule>("WorkspaceEditor");
		WorkspaceEditorModule.OpenWorkspaceForObject(Asset, EOpenWorkspaceMethod::Default, UAnimNextWorkspaceFactory::StaticClass());
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE