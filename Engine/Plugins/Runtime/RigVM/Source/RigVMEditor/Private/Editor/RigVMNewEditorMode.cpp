// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigVMNewEditorMode.h"

#include "Editor/RigVMDetailsInspectorTabSummoner.h"
#include "Editor/RigVMCompilerResultsTabSummoner.h"
#include "Editor/RigVMEditor.h"
#include "Editor/RigVMExecutionStackTabSummoner.h"
#include "Editor/RigVMFindReferencesTabSummoner.h"
#include "Editor/RigVMNewEditor.h"

const FName FRigVMNewEditorApplicationModes::StandardRigVMEditorMode() { static FName Mode = TEXT("GraphName"); return Mode; };
const FName FRigVMNewEditorApplicationModes::RigVMDefaultsMode() { static FName Mode = TEXT("DefaultsName"); return Mode; };

FRigVMNewEditorMode::FRigVMNewEditorMode(const TSharedRef<FRigVMNewEditor>& InRigVMEditor)
	: FApplicationMode(FRigVMEditorModes::RigVMEditorMode, FRigVMEditorModes::GetLocalizedMode)
{
	Editor = InRigVMEditor;
	
	TabFactories.RegisterFactory(MakeShared<FRigVMExecutionStackTabSummoner>(InRigVMEditor));
	TabFactories.RegisterFactory(MakeShared<FRigVMEditorGraphExplorerTabSummoner>(InRigVMEditor));
	TabFactories.RegisterFactory(MakeShared<FRigVMDetailsInspectorTabSummoner>(InRigVMEditor));
	TabFactories.RegisterFactory(MakeShared<FRigVMCompilerResultsTabSummoner>(InRigVMEditor));
	TabFactories.RegisterFactory(MakeShared<FRigVMFindReferencesTabSummoner>(InRigVMEditor));

	TabLayout = FTabManager::NewLayout("Standalone_RigVMEditMode_Layout_v1.5")
		->AddArea
		(
			// Main application area
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					//	Left bottom - rig/hierarchy
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->AddTab(FRigVMExecutionStackTabSummoner::TabID, ETabState::OpenedTab)
					->AddTab(FRigVMEditorGraphExplorerTabSummoner::TabID(), ETabState::OpenedTab)
				)
				->Split
				(
					// Middle 
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.6f)
					->Split
					(
						// Middle top
						FTabManager::NewStack()
						->SetSizeCoefficient(0.8f)
						->AddTab("Document", ETabState::ClosedTab)
					)
					->Split
						(
							// Middle bottom - compiler results & find
							FTabManager::NewStack()
							->SetSizeCoefficient(0.2f)
							->AddTab(FRigVMCompilerResultsTabSummoner::TabID(), ETabState::OpenedTab)
							->AddTab(FRigVMFindReferencesTabSummoner::TabID(), ETabState::OpenedTab)
						)
				)
				->Split
				(
					// Right side
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					(
						// Right top
						FTabManager::NewStack()
						->SetHideTabWell(false)
						->SetSizeCoefficient(1.f)
						->AddTab(FRigVMDetailsInspectorTabSummoner::TabID(), ETabState::OpenedTab)
						->SetForegroundTab(FRigVMDetailsInspectorTabSummoner::TabID())
					)
				)
			)
		);
}

void FRigVMNewEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FRigVMNewEditor> BP = Editor.Pin();

	// Mode-specific setup
	BP->PushTabFactories(TabFactories);
}

void FRigVMNewEditorMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	TSharedPtr<FRigVMNewEditor> BP = Editor.Pin();
	BP->RestoreEditedObjectState();
	BP->SetupViewForBlueprintEditingMode();

	FApplicationMode::PostActivateMode();
}
