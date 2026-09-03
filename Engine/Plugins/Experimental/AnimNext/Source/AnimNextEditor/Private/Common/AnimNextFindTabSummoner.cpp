// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/AnimNextFindTabSummoner.h"

#include "AnimNextRigVMAsset.h"
#include "FindInAnimNextRigVMAsset.h"
#include "IAnimNextEditorModule.h"
#include "IWorkspaceEditor.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "WorkspaceTabSummoner"

namespace UE::AnimNext::Editor
{

FAnimNextFindTabSummoner::FAnimNextFindTabSummoner(TSharedPtr<UE::Workspace::IWorkspaceEditor> InHostingApp)
	: FWorkflowTabFactory(UE::AnimNext::Editor::FindTabName, StaticCastSharedPtr<FAssetEditorToolkit>(InHostingApp))
{
	TabLabel = LOCTEXT("UAFFindTabLabel", "Find");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.Tabs.FindResults");
	ViewMenuDescription = LOCTEXT("UAFFindTabMenuDescription", "Find");
	ViewMenuTooltip = LOCTEXT("UAFFindTabToolTip", "Search contents of currently selected UAF Asset.");
	bIsSingleton = true;

	AnimNextFindResultsWidget = SNew(SFindInAnimNextRigVMAsset, InHostingApp);
}

TSharedRef<SWidget> FAnimNextFindTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return AnimNextFindResultsWidget.ToSharedRef();
}

FText FAnimNextFindTabSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return ViewMenuTooltip;
}

} // end namespace UE::AnimNext::Editor

#undef LOCTEXT_NAMESPACE
