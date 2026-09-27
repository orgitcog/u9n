// Copyright Epic Games, Inc. All Rights Reserved.

#include "IMorphTargetEditingToolInterface.h"

#include "ContextObjectStore.h"
#include "IMorphTargetEditingToolInterface.h"
#include "InteractiveToolManager.h"
#include "IPersonaEditorModeManager.h"
#include "MorphTargetEditingToolProperties.h"
#include "PersonaModule.h"
#include "SingleSelectionTool.h"
#include "SKMMorphTargetBackedTarget.h"
#include "SkeletalMesh/SkeletalMeshEditionInterface.h"

void IMorphTargetEditingToolInterface::SetupMorphEditingToolCommon()
{
	// Only support single selection tools at the moment
	USingleSelectionTool* SingleTargetTool = CastChecked<USingleSelectionTool>(this);
	ISkeletalMeshEditingInterface* EditingInterface = CastChecked<ISkeletalMeshEditingInterface>(this);
	
	if (USkeletalMeshEditorContextObjectBase* EditorContext = SingleTargetTool->GetToolManager()->GetContextObjectStore()->FindContext<USkeletalMeshEditorContextObjectBase>())
	{
		EditorContext->BindTo(EditingInterface);
	}
	
	ISkeletalMeshMorphTargetBackedTarget* MorphTargetBackedTarget = Cast<ISkeletalMeshMorphTargetBackedTarget>(SingleTargetTool->GetTarget());

	auto SetupFunction = [&](UMorphTargetEditingToolProperties* InProperties)
	{
		InProperties->MorphTargetNames = MorphTargetBackedTarget->GetEditableMorphTargetNames();
		TArray<FName> SelectedMorphTargets = EditingInterface->GetSelectedMorphTargets();
		
		InProperties->EditMorphTargetName = SelectedMorphTargets.Num() > 0 ?
			SelectedMorphTargets[0] : InProperties->MorphTargetNames.Num() > 0 ? InProperties->MorphTargetNames[0] : NAME_None;
		
		InProperties->NewMorphTargetName = MorphTargetBackedTarget->GetValidNameForNewMorphTarget(TEXT("NewMorphTarget"));

		if (SelectedMorphTargets.Num() > 0)
		{
			InProperties->Operation = EMorphTargetEditorOperation::Edit;
			MorphTargetBackedTarget->SetEditingMorphTargetName(InProperties->EditMorphTargetName);
		}
		else
		{
			InProperties->Operation = EMorphTargetEditorOperation::New;
			MorphTargetBackedTarget->SetEditingMorphTargetName(InProperties->NewMorphTargetName);
		}
		
	};
	
	SetupCommonProperties(SetupFunction);
}

void IMorphTargetEditingToolInterface::ShutdownMorphEditingToolCommon()
{
	ISkeletalMeshEditingInterface* EditingInterface = CastChecked<ISkeletalMeshEditingInterface>(this);

	// Only support single selection tools at the moment
	USingleSelectionTool* SingleTargetTool = CastChecked<USingleSelectionTool>(this);

	if (USkeletalMeshEditorContextObjectBase* EditorContext = SingleTargetTool->GetToolManager()->GetContextObjectStore()->FindContext<USkeletalMeshEditorContextObjectBase>())
	{
		EditorContext->UnbindFrom(EditingInterface);
	}	
}
