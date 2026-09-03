// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorMakeupTool.h"
#include "MetaHumanCharacterEditorActor.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ToolTargetManager.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolChange.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

class FMakeupToolCommandChange : public FToolCommandChange
{
public:

	FMakeupToolCommandChange(const FMetaHumanCharacterMakeupSettings& InOldMakeupSettings,
							 const FMetaHumanCharacterMakeupSettings& InNewMakeupSetgings,
							 TNotNull<UInteractiveToolManager*> InToolManager)
		: OldMakeupSettings{ InOldMakeupSettings }
		, NewMakeupSettings{ InNewMakeupSetgings }
		, ToolManager{ InToolManager }
	{
	}

	//~Begin FToolCommandChange interface
	virtual FString ToString() const override
	{
		return TEXT("MetaHuman Character Edit Makeup");
	}

	virtual bool HasExpired(UObject* InObject) const override
	{
		return !ToolManager.IsValid();
	}

	void Apply(UObject* InObject) override
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);

		UMetaHumanCharacterEditorSubsystem::Get()->CommitMakeupSettings(Character, NewMakeupSettings);

		UpdateMakeupToolProperties(NewMakeupSettings);
	}

	void Revert(UObject* InObject) override
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);

		UMetaHumanCharacterEditorSubsystem::Get()->CommitMakeupSettings(Character, OldMakeupSettings);

		UpdateMakeupToolProperties(OldMakeupSettings);
	}
	//~End FToolCommandChange interface

protected:

	void UpdateMakeupToolProperties(const FMetaHumanCharacterMakeupSettings& InMakeupSettings)
	{
		if (ToolManager.IsValid())
		{
			if (UMetaHumanCharacterEditorMakeupTool* MakeupTool = Cast<UMetaHumanCharacterEditorMakeupTool>(ToolManager->GetActiveTool(EToolSide::Left)))
			{
				UMetaHumanCharacterEditorMakeupToolProperties* MakeupToolProperties = nullptr;
				if (MakeupTool->GetToolProperties().FindItemByClass<UMetaHumanCharacterEditorMakeupToolProperties>(&MakeupToolProperties))
				{
					MakeupToolProperties->CopyFrom(InMakeupSettings);
					MakeupToolProperties->SilentUpdateWatched();

					MakeupTool->PreviousMakeupSettings = InMakeupSettings;
				}
			}
		}
	}

protected:

	FMetaHumanCharacterMakeupSettings OldMakeupSettings;
	FMetaHumanCharacterMakeupSettings NewMakeupSettings;

	TWeakObjectPtr<UInteractiveToolManager> ToolManager;

};

bool UMetaHumanCharacterEditorMakeupToolBuilder::CanBuildTool(const FToolBuilderState& InSceneState) const
{
	const int32 NumTargets = InSceneState.TargetManager->CountSelectedAndTargetableWithPredicate(InSceneState, GetTargetRequirements(), [](UActorComponent& Component)
	{
		return Component.GetOwner()->Implements<UMetaHumanCharacterEditorActorInterface>();
	});

	// Restrict the tool to a single target
	return NumTargets == 1;
}

UInteractiveTool* UMetaHumanCharacterEditorMakeupToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	UMetaHumanCharacterEditorMakeupTool* MakeupTool = NewObject<UMetaHumanCharacterEditorMakeupTool>(InSceneState.ToolManager);
	MakeupTool->SetTarget(Target);

	return MakeupTool;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorMakeupToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass(),
		}
	);

	return TypeRequirements;
}

void UMetaHumanCharacterEditorMakeupToolProperties::CopyFrom(const FMetaHumanCharacterMakeupSettings& InMakeupSettings)
{
	Foundation = InMakeupSettings.Foundation;
	Eyes = InMakeupSettings.Eyes;
	Blush = InMakeupSettings.Blush;
	Lips = InMakeupSettings.Lips;
}

void UMetaHumanCharacterEditorMakeupToolProperties::CopyTo(FMetaHumanCharacterMakeupSettings& OutMakeupSettings)
{
	OutMakeupSettings.Foundation = Foundation;
	OutMakeupSettings.Eyes = Eyes;
	OutMakeupSettings.Blush = Blush;
	OutMakeupSettings.Lips = Lips;
}

void UMetaHumanCharacterEditorMakeupTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(LOCTEXT("MakeupToolName", "Makeup"));

	MakeupProperties = NewObject<UMetaHumanCharacterEditorMakeupToolProperties>(this);
	AddToolPropertySource(MakeupProperties);

	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	PreviousMakeupSettings = Character->MakeupSettings;
	MakeupProperties->CopyFrom(PreviousMakeupSettings);

	// Auto select skin preview
	if (Character->PreviewMaterialType != EMetaHumanCharacterSkinPreviewMaterial::Editable)
	{
		UMetaHumanCharacterEditorSubsystem::Get()->UpdateCharacterPreviewMaterial(Character, EMetaHumanCharacterSkinPreviewMaterial::Editable);
	}
}

void UMetaHumanCharacterEditorMakeupTool::Shutdown(EToolShutdownType InShutdownType)
{
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	if (bActorWasModified)
	{
		FMetaHumanCharacterMakeupSettings CurrentMakeupSettings;
		MakeupProperties->CopyTo(CurrentMakeupSettings);

		UMetaHumanCharacterEditorSubsystem::Get()->CommitMakeupSettings(Character, CurrentMakeupSettings);

		// Add the undo command
		TUniquePtr<FMakeupToolCommandChange> CommandChange = MakeUnique<FMakeupToolCommandChange>(PreviousMakeupSettings, CurrentMakeupSettings, GetToolManager());
		GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("MakeupToolCommandChangeTransaction", "Edit Makeup"));
	}
}

void UMetaHumanCharacterEditorMakeupTool::OnPropertyModified(UObject* InPropertySet, FProperty* InProperty)
{
	if (InPropertySet == MakeupProperties)
	{
		if (UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target))
		{
			UpdateMakeupSettings();

			FMetaHumanCharacterMakeupSettings NewMakeupSettings;
			MakeupProperties->CopyTo(NewMakeupSettings);

			TUniquePtr<FMakeupToolCommandChange> CommandChange = MakeUnique<FMakeupToolCommandChange>(PreviousMakeupSettings, NewMakeupSettings, GetToolManager());
			GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("MakeupToolCommandChange", "Edit Makeup"));

			PreviousMakeupSettings = NewMakeupSettings;
			bActorWasModified = true;
		}
	}
}

void UMetaHumanCharacterEditorMakeupTool::UpdateMakeupSettings()
{
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	FMetaHumanCharacterMakeupSettings NewMakeupSettings;
	MakeupProperties->CopyTo(NewMakeupSettings);

	UMetaHumanCharacterEditorSubsystem::Get()->ApplyMakeupSettings(Character, NewMakeupSettings);
}

#undef LOCTEXT_NAMESPACE