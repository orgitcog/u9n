// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorCostumeTools.h"

#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorActorInterface.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanCollection.h"
#include "MetaHumanWardrobeItem.h"

#include "Editor/EditorEngine.h"
#include "InteractiveToolManager.h"
#include "SceneManagement.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "ToolTargetManager.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

extern UNREALED_API UEditorEngine* GEditor;

bool UMetaHumanCharacterEditorCostumeToolBuilder::CanBuildTool(const FToolBuilderState& InSceneState) const
{
	const int32 NumTargets = InSceneState.TargetManager->CountSelectedAndTargetableWithPredicate(InSceneState, GetTargetRequirements(), [](UActorComponent& Component)
	{
		return Component.GetOwner()->Implements<UMetaHumanCharacterEditorActorInterface>();
	});

	// Restrict the tool to a single target
	return NumTargets == 1;
}

UInteractiveTool* UMetaHumanCharacterEditorCostumeToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	switch (ToolType)
	{
		case EMetaHumanCharacterCostumeEditingTool::Costume:
		{
			UMetaHumanCharacterEditorCostumeTool* CostumeTool = NewObject<UMetaHumanCharacterEditorCostumeTool>(InSceneState.ToolManager);
			CostumeTool->SetTarget(Target);
			CostumeTool->SetTargetWorld(InSceneState.World);
			return CostumeTool;
		}

		default:
			checkNoEntry();
	}

	return nullptr;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorCostumeToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass()
		}
	);

	return TypeRequirements;
}

void UMetaHumanCharacterEditorCostumeTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(LOCTEXT("CostumeToolName", "Costume"));

	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	PropertyObject = NewObject<UMetaHumanCharacterEditorCostumeToolProperties>();
	PropertyObject->Collection = Character->GetMutableInternalCollection();

	AddToolPropertySource(PropertyObject);

	UpdateCostumeItems();
}

void UMetaHumanCharacterEditorCostumeTool::UpdateCostumeItems()
{
	if (!PropertyObject || !PropertyObject->Collection)
	{
		return;
	}

	TArray<FMetaHumanCharacterEditorCostumeItem>& CostumeItems = PropertyObject->CostumeItems;
	CostumeItems.Reset();

	TNotNull<const UMetaHumanCharacterInstance*> MetaHumanInstance = PropertyObject->Collection->GetDefaultInstance();
	const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections = MetaHumanInstance->GetSlotSelectionData();
	for (const FMetaHumanPipelineSlotSelectionData& SlotSelection : SlotSelections)
	{
		if (SlotSelection.Selection.SlotName == UE::MetaHuman::CharacterPipelineSlots::Character)
		{
			continue;
		}

		const UMetaHumanCharacterPalette* ContainingPalette = nullptr;
		FMetaHumanCharacterPaletteItem Item;
		if (!PropertyObject->Collection->TryResolveItem(SlotSelection.Selection.GetSelectedItemPath(), ContainingPalette, Item)
			|| !Item.WardrobeItem)
		{
			continue;
		}

		FMetaHumanCharacterEditorCostumeItem& NewItem = CostumeItems.AddDefaulted_GetRef();
		NewItem.SlotName = SlotSelection.Selection.SlotName;
		NewItem.ItemPath = SlotSelection.Selection.GetSelectedItemPath();
		NewItem.WardrobeItem = Item.WardrobeItem;
		NewItem.InstanceParameters = MetaHumanInstance->GetCurrentInstanceParametersForItem(SlotSelection.Selection.GetSelectedItemPath());
	}
}

#undef LOCTEXT_NAMESPACE 
