// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHUmanCharacterEditorEyesTool.h"
#include "MetaHumanCharacterEditorActor.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ToolTargetManager.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolChange.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

class FEyesToolCommandChange : public FToolCommandChange
{
public:

	FEyesToolCommandChange(const FMetaHumanCharacterEyesSettings& InOldEyeSettings,
						   const FMetaHumanCharacterEyesSettings& InNewEyeSettings,
						   TNotNull<UInteractiveToolManager*> InToolManager)
		: OldEyesSettings{ InOldEyeSettings }
		, NewEyesSettings{ InNewEyeSettings }
		, ToolManager(InToolManager)
	{
	}

	//~Begin FToolCommandChange interface
	virtual FString ToString() const override
	{
		return TEXT("MetaHuman Character Edit Eyes");
	}

	virtual bool HasExpired(UObject* InObject) const override
	{
		return !ToolManager.IsValid();
	}

	virtual void Apply(UObject* InObject) override
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);

		UMetaHumanCharacterEditorSubsystem::Get()->CommitEyesSettings(Character, NewEyesSettings);

		UpdateEyesToolProperties(NewEyesSettings);
	}

	virtual void Revert(UObject* InObject) override
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);

		UMetaHumanCharacterEditorSubsystem::Get()->CommitEyesSettings(Character, OldEyesSettings);

		UpdateEyesToolProperties(OldEyesSettings);
	}
	//~End FToolCommandChange interface

protected:

	void UpdateEyesToolProperties(const FMetaHumanCharacterEyesSettings& InEyesSettings)
	{
		if (ToolManager.IsValid())
		{
			if (UMetaHumanCharacterEditorEyesTool* EyesTool = Cast<UMetaHumanCharacterEditorEyesTool>(ToolManager->GetActiveTool(EToolSide::Left)))
			{
				UMetaHumanCharacterEditorEyesToolProperties* EyesToolProperties = nullptr;
				if (EyesTool->GetToolProperties().FindItemByClass<UMetaHumanCharacterEditorEyesToolProperties>(&EyesToolProperties))
				{
					EyesToolProperties->CopyFrom(InEyesSettings);
					EyesToolProperties->SilentUpdateWatched();
				}
			}
		}
	}

private:

	FMetaHumanCharacterEyesSettings OldEyesSettings;
	FMetaHumanCharacterEyesSettings NewEyesSettings;

	TWeakObjectPtr<UInteractiveToolManager> ToolManager;
};

TNotNull<UMetaHumanCharacterEyePresets*> UMetaHumanCharacterEyePresets::Get()
{
	return LoadObject<UMetaHumanCharacterEyePresets>(nullptr, TEXT("/Script/MetaHumanCharacterEditor.MetaHumanCharacterEyePresets'/" UE_PLUGIN_NAME "/Tools/EyePresets/EyePresets.EyePresets'"));
}

bool UMetaHumanCharacterEditorEyesToolBuilder::CanBuildTool(const FToolBuilderState& InSceneState) const
{
	const int32 NumTargets = InSceneState.TargetManager->CountSelectedAndTargetableWithPredicate(InSceneState, GetTargetRequirements(), [](UActorComponent& Component)
	{
		return Component.GetOwner()->Implements<UMetaHumanCharacterEditorActorInterface>();
	});

	// Restrict the tool to a single target
	return NumTargets == 1;
}

UInteractiveTool* UMetaHumanCharacterEditorEyesToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	UMetaHumanCharacterEditorEyesTool* EyesTool = NewObject<UMetaHumanCharacterEditorEyesTool>(InSceneState.ToolManager);
	EyesTool->SetTarget(Target);

	return EyesTool;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorEyesToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass(),
		}
	);

	return TypeRequirements;
}

void UMetaHumanCharacterEditorEyesToolProperties::CopyFrom(const FMetaHumanCharacterEyesSettings& InEyesSettings)
{
	if (EyeSelection == EMetaHumanCharacterEyeEditSelection::Left || EyeSelection == EMetaHumanCharacterEyeEditSelection::Both)
	{
		Eye = InEyesSettings.EyeLeft;
	}
	else if (EyeSelection == EMetaHumanCharacterEyeEditSelection::Right)
	{
		Eye = InEyesSettings.EyeRight;
	}
}

void UMetaHumanCharacterEditorEyesToolProperties::CopyTo(FMetaHumanCharacterEyesSettings& OutEyesSettings) const
{
	if (EyeSelection == EMetaHumanCharacterEyeEditSelection::Left || EyeSelection == EMetaHumanCharacterEyeEditSelection::Both)
	{
		OutEyesSettings.EyeLeft = Eye;
	}
	
	if (EyeSelection == EMetaHumanCharacterEyeEditSelection::Right || EyeSelection == EMetaHumanCharacterEyeEditSelection::Both)
	{
		OutEyesSettings.EyeRight = Eye;
	}
}

UMetaHumanCharacterEditorEyesToolProperties* UMetaHumanCharacterEditorEyesTool::GetEyesToolProperties() const
{
	return EyesProperties;
}

void UMetaHumanCharacterEditorEyesTool::SetEyeSelection(EMetaHumanCharacterEyeEditSelection InSelection)
{
	EyesProperties->EyeSelection = InSelection;

	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	EyesProperties->CopyFrom(Character->EyesSettings);
}

void UMetaHumanCharacterEditorEyesTool::SetEyesFromPreset(const FMetaHumanCharacterEyesSettings& InPreset)
{
	if (UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target))
	{
		// Update the eye properties with the values from the preset
		EyesProperties->CopyFrom(InPreset);

		TUniquePtr<FEyesToolCommandChange> CommandChange = MakeUnique<FEyesToolCommandChange>(Character->EyesSettings, InPreset, GetToolManager());
		GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("EyeToolSetPresetCommandChange", "Set Eyes Preset"));

		UMetaHumanCharacterEditorSubsystem::Get()->CommitEyesSettings(Character, InPreset);
	}
}

void UMetaHumanCharacterEditorEyesTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(LOCTEXT("EyesToolName", "Eyes"));

	EyesProperties = NewObject<UMetaHumanCharacterEditorEyesToolProperties>(this);
	AddToolPropertySource(EyesProperties);

	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	EyesProperties->RestoreProperties(this);

	EyesProperties->CopyFrom(Character->EyesSettings);

	// Auto select skin preview
	if (Character->PreviewMaterialType == EMetaHumanCharacterSkinPreviewMaterial::Default)
	{
		UMetaHumanCharacterEditorSubsystem::Get()->UpdateCharacterPreviewMaterial(Character, EMetaHumanCharacterSkinPreviewMaterial::Editable);
	}
}

void UMetaHumanCharacterEditorEyesTool::Shutdown(EToolShutdownType InShutdownType)
{
	Super::Shutdown(InShutdownType);

	EyesProperties->SaveProperties(this);
}

void UMetaHumanCharacterEditorEyesTool::OnPropertyModified(UObject* InPropertySet, FProperty* InProperty)
{
	if (InPropertySet == EyesProperties)
	{
		if (UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target))
		{
			FMetaHumanCharacterEyesSettings NewEyesSettings = Character->EyesSettings;
			EyesProperties->CopyTo(NewEyesSettings);

			TUniquePtr<FEyesToolCommandChange> CommandChange = MakeUnique<FEyesToolCommandChange>(Character->EyesSettings, NewEyesSettings, GetToolManager());
			GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("EyesToolCommandChange", "Edit Eyes"));

			UMetaHumanCharacterEditorSubsystem::Get()->CommitEyesSettings(Character, NewEyesSettings);
		}
	}
}

#undef LOCTEXT_NAMESPACE
