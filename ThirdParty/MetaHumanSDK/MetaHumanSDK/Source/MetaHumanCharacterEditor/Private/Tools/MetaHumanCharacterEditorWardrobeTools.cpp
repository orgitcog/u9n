// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorWardrobeTools.h"

#include "Editor/EditorEngine.h"
#include "InteractiveToolManager.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterEditorActorInterface.h"
#include "MetaHumanCharacterEditorSettings.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCollection.h"
#include "SceneManagement.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "ToolTargetManager.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

bool UMetaHumanCharacterEditorWardrobeToolBuilder::CanBuildTool(const FToolBuilderState& InSceneState) const
{
	const int32 NumTargets = InSceneState.TargetManager->CountSelectedAndTargetableWithPredicate(InSceneState, GetTargetRequirements(), [](UActorComponent& Component)
	{
		return Component.GetOwner()->Implements<UMetaHumanCharacterEditorActorInterface>();
	});

	// Restrict the tool to a single target
	return NumTargets == 1;
}

UInteractiveTool* UMetaHumanCharacterEditorWardrobeToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	switch (ToolType)
	{
		case EMetaHumanCharacterWardrobeEditingTool::Wardrobe:
		{
			UMetaHumanCharacterEditorWardrobeTool* WardrobeTool = NewObject<UMetaHumanCharacterEditorWardrobeTool>(InSceneState.ToolManager);
			WardrobeTool->SetTarget(Target);
			WardrobeTool->SetTargetWorld(InSceneState.World);
			return WardrobeTool;
		}

		default:
			checkNoEntry();
	}

	return nullptr;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorWardrobeToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass()
		}
	);

	return TypeRequirements;
}

void UMetaHumanCharacterEditorWardrobeTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(LOCTEXT("WardrobeToolName", "Wardrobe"));

	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	PropertyObject = NewObject<UMetaHumanCharacterEditorWardrobeToolProperties>(this);
	PropertyObject->Collection = Character->GetMutableInternalCollection();
	PropertyObject->Character = Character;

	AddToolPropertySource(PropertyObject);

	// Rebuild tool when the character's wardrobe paths change
	WardrobePathChangedCharacter = Character->OnWardrobePathsChanged.AddUObject(
		this,
		&UMetaHumanCharacterEditorWardrobeTool::OnWardrobePathsChanged);

	// Rebuild tool when the user settings' wardrobe paths change
	WardrobePathChangedUserSettings = GetMutableDefault<UMetaHumanCharacterEditorSettings>()->OnWardrobePathsChanged.AddUObject(
		this,
		&UMetaHumanCharacterEditorWardrobeTool::OnWardrobePathsChanged);
}

void UMetaHumanCharacterEditorWardrobeTool::Shutdown(EToolShutdownType InShutdownType)
{
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	if (Character)
	{
		Character->OnWardrobePathsChanged.Remove(WardrobePathChangedCharacter);
		WardrobePathChangedCharacter.Reset();
	}

	GetMutableDefault<UMetaHumanCharacterEditorSettings>()->OnWardrobePathsChanged.Remove(WardrobePathChangedUserSettings);
	WardrobePathChangedUserSettings.Reset();
}

void UMetaHumanCharacterEditorWardrobeTool::OnWardrobePathsChanged()
{
	// Reactivate the same tool, the previous one will shut down
	GetToolManager()->ActivateTool(EToolSide::Left);
}

#undef LOCTEXT_NAMESPACE 
