// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigInfluenceMapTabSummoner.h"
#include "Editor/SRigHierarchy.h"
#include "Editor/RigVMEditorStyle.h"
#include "Editor/ControlRigEditor.h"
#include "ControlRigBlueprint.h"

#if WITH_RIGVMLEGACYEDITOR
#include "SKismetInspector.h"
#else
#include "Editor/SRigVMDetailsInspector.h"
#endif

#define LOCTEXT_NAMESPACE "RigInfluenceMapTabSummoner"

const FName FRigInfluenceMapTabSummoner::TabID(TEXT("RigInfluenceMap"));

FRigInfluenceMapTabSummoner::FRigInfluenceMapTabSummoner(const TSharedRef<IControlRigBaseEditor>& InControlRigEditor)
	: FWorkflowTabFactory(TabID, InControlRigEditor->GetHostingApp())
	, ControlRigEditor(InControlRigEditor)
{
	TabLabel = LOCTEXT("RigInfluenceMapTabLabel", "Rig Influence Map");
	TabIcon = FSlateIcon(FRigVMEditorStyle::Get().GetStyleSetName(), "RigVM.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("RigInfluenceMap_ViewMenu_Desc", "Rig Influence Map");
	ViewMenuTooltip = LOCTEXT("RigInfluenceMap_ViewMenu_ToolTip", "Show the Rig Influence Map tab");
}

TSharedRef<SWidget> FRigInfluenceMapTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
#if WITH_RIGVMLEGACYEDITOR
	TSharedRef<SKismetInspector> KismetInspector = SNew(SKismetInspector);
#else
	TSharedRef<SRigVMDetailsInspector> KismetInspector = SNew(SRigVMDetailsInspector);
#endif

	if (TSharedPtr<IControlRigBaseEditor> Editor = ControlRigEditor.Pin())
	{
		if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(Editor->GetControlRigBlueprint()))
		{
			TSharedPtr<FStructOnScope> StructToDisplay = MakeShareable(new FStructOnScope(
				FRigInfluenceMapPerEvent::StaticStruct(), (uint8*)&RigBlueprint->Influences));
			StructToDisplay->SetPackage(RigBlueprint->GetOutermost());
			KismetInspector->ShowSingleStruct(StructToDisplay);
		}
	}

	return KismetInspector;
}

#undef LOCTEXT_NAMESPACE 
