// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigValidationTabSummoner.h"
#include "Editor/SRigHierarchy.h"
#include "ControlRigEditorStyle.h"
#include "Editor/ControlRigEditor.h"
#include "ControlRigBlueprint.h"
#include "Editor/SControlRigValidationWidget.h"

#define LOCTEXT_NAMESPACE "RigValidationTabSummoner"

const FName FRigValidationTabSummoner::TabID(TEXT("RigValidation"));

FRigValidationTabSummoner::FRigValidationTabSummoner(const TSharedRef<IControlRigBaseEditor>& InControlRigEditor)
	: FWorkflowTabFactory(TabID, InControlRigEditor->GetHostingApp())
	, WeakControlRigEditor(InControlRigEditor)
{
	TabLabel = LOCTEXT("RigValidationTabLabel", "Rig Validation");
	TabIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "RigValidation.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("RigValidation_ViewMenu_Desc", "Rig Validation");
	ViewMenuTooltip = LOCTEXT("RigValidation_ViewMenu_ToolTip", "Show the Rig Validation tab");
}

TSharedRef<SWidget> FRigValidationTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	ensure(WeakControlRigEditor.IsValid());
	TSharedRef<IRigVMEditor> EditorRef = WeakControlRigEditor.Pin()->SharedRigVMEditorRef();
	TSharedPtr<FRigVMEditorBase> EditorPtr = StaticCastSharedPtr<FRigVMEditorBase>(EditorRef.ToSharedPtr());
	
	if (TSharedPtr<FRigVMEditorBase> Editor = EditorPtr)
	{
		UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(Editor->GetRigVMBlueprint());
		check(RigBlueprint);
	
		UControlRigValidator* Validator = RigBlueprint->Validator;
		check(Validator);

		TSharedRef<SControlRigValidationWidget> ValidationWidget = SNew(SControlRigValidationWidget, Validator);
		Validator->SetControlRig(Cast<UControlRig>(RigBlueprint->GetObjectBeingDebugged()));
		return ValidationWidget;
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE 
