// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextStateTreeEditorHost.h"

#include "AnimNextStateTree.h"
#include "IAnimNextEditorModule.h"
#include "StateTree.h"
#include "StateTreeEditorWorkspaceTabHost.h"

#include "IWorkspaceEditor.h"

void FAnimNextStateTreeEditorHost::Init(const TWeakPtr<UE::Workspace::IWorkspaceEditor>& InWeakWorkspaceEditor)
{
	WeakWorkspaceEditor = InWeakWorkspaceEditor;
	TabHost = MakeShared<UE::StateTreeEditor::FWorkspaceTabHost>();
	
	TSharedPtr<UE::Workspace::IWorkspaceEditor> SharedEditor = InWeakWorkspaceEditor.Pin();
	check(SharedEditor.IsValid());
	SharedEditor->OnFocussedDocumentChanged().AddSP(this, &FAnimNextStateTreeEditorHost::OnWorkspaceFocussedDocumentChanged);
}

UStateTree* FAnimNextStateTreeEditorHost::GetStateTree() const
{
	if (TSharedPtr<UE::Workspace::IWorkspaceEditor> SharedWorkspaceEditor = WeakWorkspaceEditor.Pin())
	{
		const TObjectPtr<UAnimNextStateTree> AnimNextStateTreePtr = SharedWorkspaceEditor->GetFocussedDocument<UAnimNextStateTree>();
		return AnimNextStateTreePtr ? AnimNextStateTreePtr->StateTree : nullptr;
	}	

	return nullptr;
}

FName FAnimNextStateTreeEditorHost::GetCompilerLogName() const
{
	return UE::AnimNext::Editor::LogListingName;
}

FName FAnimNextStateTreeEditorHost::GetCompilerTabName() const
{
	return UE::AnimNext::Editor::CompilerResultsTabName;
}

bool FAnimNextStateTreeEditorHost::ShouldShowCompileButton() const
{
	return false;
}

bool FAnimNextStateTreeEditorHost::CanToolkitSpawnWorkspaceTab() const
{
	return true;
}

void FAnimNextStateTreeEditorHost::OnWorkspaceFocussedDocumentChanged(TObjectPtr<UObject> InObject) const
{
	TObjectPtr<UAnimNextStateTree> InStateTree = Cast<UAnimNextStateTree>(InObject);

	if (TStrongObjectPtr<UAnimNextStateTree> LastStateTreePinned = WeakLastStateTree.Pin())
	{
		if (LastStateTreePinned != InStateTree)
		{
			OnStateTreeChangedDelegate.Broadcast();
		}
	}
	else if (InStateTree)
	{
		OnStateTreeChangedDelegate.Broadcast();
	}

	// Always set the last state tree as incoming object after cast. If it's null that will clear the last state tree
	WeakLastStateTree = InStateTree;
}

FSimpleMulticastDelegate& FAnimNextStateTreeEditorHost::OnStateTreeChanged()
{
	return OnStateTreeChangedDelegate;
}

TSharedPtr<IDetailsView> FAnimNextStateTreeEditorHost::GetAssetDetailsView()
{
	if (TSharedPtr<UE::Workspace::IWorkspaceEditor> SharedEditor = WeakWorkspaceEditor.Pin())
	{
		return SharedEditor->GetDetailsView();
	}
	
	return nullptr;
}

TSharedPtr<IDetailsView> FAnimNextStateTreeEditorHost::GetDetailsView()
{
	if (TSharedPtr<UE::Workspace::IWorkspaceEditor> SharedEditor = WeakWorkspaceEditor.Pin())
	{
		return SharedEditor->GetDetailsView();
	}
	
	return nullptr;
}

TSharedPtr<UE::StateTreeEditor::FWorkspaceTabHost> FAnimNextStateTreeEditorHost::GetTabHost() const
{
	return TabHost;
}
