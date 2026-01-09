// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitStackEditor.h"
#include "IWorkspaceEditor.h"
#include "Graph/TraitEditorTabSummoner.h"
#include "Widgets/Docking/SDockTab.h"

namespace UE::AnimNext::Editor
{

void FTraitStackEditor::SetTraitData(const TSharedRef<UE::Workspace::IWorkspaceEditor>& InWorkspaceEditor, const FTraitStackData& InTraitStackData)
{
	if (TSharedPtr<SDockTab> DockTab = InWorkspaceEditor->GetTabManager()->FindExistingLiveTab(FTabId(TraitEditorTabName)))
	{
		if (TSharedPtr<STraitEditorView> TraitEditorView = StaticCastSharedPtr<STraitEditorView>(DockTab->GetContent().ToSharedPtr()))
		{
			TraitEditorView->SetTraitData(InTraitStackData);
		}
	}
}

}
