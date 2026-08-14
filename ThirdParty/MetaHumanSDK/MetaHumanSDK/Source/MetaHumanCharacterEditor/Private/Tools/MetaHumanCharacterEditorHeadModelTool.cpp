// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorHeadModelTool.h"

#include "Editor/EditorEngine.h"
#include "InteractiveToolManager.h"
#include "MetaHumanCharacterEditorActor.h"
#include "MetaHumanCharacterEditorCommands.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ToolTargetManager.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditorHeadModelTool"

extern UNREALED_API UEditorEngine* GEditor;

// Undo command for keeping track of changes in the Character head model settings
class FMetaHumanCharacterEditorHeadModelToolCommandChange : public FToolCommandChange
{
public:
	FMetaHumanCharacterEditorHeadModelToolCommandChange(const FMetaHumanCharacterHeadModelSettings& InOldHeadModelSettings,
		const FMetaHumanCharacterHeadModelSettings& InNewHeadModelSettings,
		TNotNull<UInteractiveToolManager*> InToolManager)
		: OldHeadModelSettings(InOldHeadModelSettings)
		, NewHeadModelSettings(InNewHeadModelSettings)
		, ToolManager{ InToolManager }
	{
	}

	//~Begin FToolCommandChange interface
	virtual FString ToString() const override
	{
		return TEXT("MetaHuman Character Edit Head Model");
	}

	virtual bool HasExpired(UObject* InObject) const override
	{
		// If the ToolManager is not valid anymore it means the asset editor was closed so mark the transaction as expired
		return !ToolManager.IsValid();
	}

	void Apply(UObject* InObject) override
	{
		UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacter>(InObject);
		GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>()->CommitHeadModelSettings(MetaHumanCharacter, NewHeadModelSettings);

		UpdateHeadModelToolProperties(NewHeadModelSettings);
	}

	void Revert(UObject* InObject) override
	{
		UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacter>(InObject);
		GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>()->CommitHeadModelSettings(MetaHumanCharacter, OldHeadModelSettings);

		UpdateHeadModelToolProperties(OldHeadModelSettings);
	}
	//~End FToolCommandChange interface

protected:

	/**
	 * Updates the Head Model Tool Properties of the active tool using the given head model settings
	 */
	void UpdateHeadModelToolProperties(const FMetaHumanCharacterHeadModelSettings& InHeadModelSettings)
	{
		if (ToolManager.IsValid())
		{
			if (UMetaHumanCharacterEditorHeadModelTool* HeadModelTool = Cast<UMetaHumanCharacterEditorHeadModelTool>(ToolManager->GetActiveTool(EToolSide::Left)))
			{
				for (UObject* ToolProperty : HeadModelTool->GetToolProperties())
				{
					if (UMetaHumanCharacterHeadModelSubToolBase* HeadModelProperty = Cast<UMetaHumanCharacterHeadModelSubToolBase>(ToolProperty))
					{
						HeadModelProperty->CopyFrom(InHeadModelSettings);
						HeadModelProperty->SilentUpdateWatched();
					}
				}

				// Restore the PreviousHeadModelSettings of the tool to what we are applying so that
				// new commands are created with the correct previous settings
				HeadModelTool->PreviousHeadModelSettings = InHeadModelSettings;
			}
		}
	}

protected:

	// Store as FMetaHumanCharacterHeadModelSettings since it is simpler to manage the lifetime of structs
	FMetaHumanCharacterHeadModelSettings OldHeadModelSettings;
	FMetaHumanCharacterHeadModelSettings NewHeadModelSettings;

	// Reference to head model tool manager, used to update the head model tool properties when applying transactions
	TWeakObjectPtr<UInteractiveToolManager> ToolManager;
};

// Specialized version of the the head model edit command that also updates the face state Eyelashes variant
class FMetaHumanCharacterEditorEyelashesTypeCommandChange : public FMetaHumanCharacterEditorHeadModelToolCommandChange
{
public:
	FMetaHumanCharacterEditorEyelashesTypeCommandChange(const FMetaHumanCharacterHeadModelSettings& InOldHeadModelSettings,
		const FMetaHumanCharacterHeadModelSettings& InNewHeadModelSettings,
		TSharedRef<FMetaHumanCharacterIdentity::FState> InReferenceFaceState,
		TNotNull<UInteractiveToolManager*> InToolManager)
		: FMetaHumanCharacterEditorHeadModelToolCommandChange(InOldHeadModelSettings, InNewHeadModelSettings, InToolManager)
		, ReferenceFaceState(InReferenceFaceState)
	{
	}

	//~Begin FToolCommandChange interface
	virtual FString ToString() const override
	{
		return TEXT("MetaHuman Character Edit Eyelashes Properties");
	}

	void Apply(UObject* InObject) override
	{
		ApplyHeadModelSettingsAndEyelashesVariant(InObject, NewHeadModelSettings);

	}

	void Revert(UObject* InObject) override
	{
		ApplyHeadModelSettingsAndEyelashesVariant(InObject, OldHeadModelSettings);
	}
	//~End FToolCommandChange interface

private:
	// State to be used for applying the Eyelashes variant from the Eyelashes type property
	TSharedRef<FMetaHumanCharacterIdentity::FState> ReferenceFaceState;


	void ApplyHeadModelSettingsAndEyelashesVariant(UObject* InObject, const FMetaHumanCharacterHeadModelSettings& InHeadModelSettings)
	{
		UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacter>(InObject);
		UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
		check(MetaHumanCharacterSubsystem);

		MetaHumanCharacterSubsystem->CommitHeadModelSettings(MetaHumanCharacter, InHeadModelSettings);

		// Copy the reference state and apply the Teeth variant
		TSharedRef<FMetaHumanCharacterIdentity::FState> NewState = MakeShared<FMetaHumanCharacterIdentity::FState>(*ReferenceFaceState);
		MetaHumanCharacterSubsystem->UpdateEyelashesVariantFromProperties(NewState, InHeadModelSettings.Eyelashes);
		MetaHumanCharacterSubsystem->CommitFaceState(MetaHumanCharacter, NewState);

		UpdateHeadModelToolProperties(InHeadModelSettings);
	}
};

UInteractiveTool* UMetaHumanCharacterEditorHeadModelToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	switch (ToolType)
	{
		case EMetaHumanCharacterHeadModelTool::Model:
		{
			UMetaHumanCharacterEditorHeadModelTool* HeadModelTool = NewObject<UMetaHumanCharacterEditorHeadModelTool>(InSceneState.ToolManager);
			HeadModelTool->SetTarget(Target);
			return HeadModelTool;
		}
		case EMetaHumanCharacterHeadModelTool::Materials:
		{
			UMetaHumanCharacterEditorHeadMaterialsTool* HeadMaterialsTool = NewObject<UMetaHumanCharacterEditorHeadMaterialsTool>(InSceneState.ToolManager);
			HeadMaterialsTool->SetTarget(Target);
			return HeadMaterialsTool;
		}
		case EMetaHumanCharacterHeadModelTool::Grooms:
		{
			// TODO: Add groom model tool for eyelashes.
			break;
		}
		default:
			check(false);
			break;
	}

	return nullptr;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorHeadModelToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass(),
		}
	);

	return TypeRequirements;
}

void UMetaHumanCharacterHeadModelEyelashesProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Override function to process EPropertyChangeType::ValueSet events for the edited properties
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// When the reset to default button is clicked in the details panel ChangeType will have both ValueSet and ResetToDefault bits set
	if ((PropertyChangedEvent.ChangeType & (EPropertyChangeType::ValueSet | EPropertyChangeType::ResetToDefault | EPropertyChangeType::Interactive)) != 0u)
	{
		bool bIsEyelashesTypeModified = false;
		// The Eyelashes Type property is handled differently since we need to update both texture and face state
		const FName PropertyName = PropertyChangedEvent.GetPropertyName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyelashesProperties, Type))
		{
			bIsEyelashesTypeModified = true;
		}

		OnEyelashesPropertyValueSetDelegate.ExecuteIfBound(bIsEyelashesTypeModified);
	}
}

void UMetaHumanCharacterHeadModelEyelashesProperties::CopyTo(FMetaHumanCharacterHeadModelSettings& OutHeadModelSettings)
{
	OutHeadModelSettings.Eyelashes = Eyelashes;
}

void UMetaHumanCharacterHeadModelEyelashesProperties::CopyFrom(const FMetaHumanCharacterHeadModelSettings& InHeadModelSettings)
{
	Eyelashes = InHeadModelSettings.Eyelashes;
}

void UMetaHumanCharacterHeadModelTeethProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Override function to process EPropertyChangeType::ValueSet events for the edited properties
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// When the reset to default button is clicked in the details panel ChangeType will have both ValueSet and ResetToDefault bits set
	if ((PropertyChangedEvent.ChangeType & (EPropertyChangeType::ValueSet | EPropertyChangeType::ResetToDefault | EPropertyChangeType::Interactive)) != 0u)
	{
		OnTeethPropertyValueSetDelegate.ExecuteIfBound(PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive);
	}
}

void UMetaHumanCharacterHeadModelTeethProperties::CopyTo(FMetaHumanCharacterHeadModelSettings& OutHeadModelSettings)
{
	OutHeadModelSettings.Teeth = Teeth;
}

void UMetaHumanCharacterHeadModelTeethProperties::CopyFrom(const FMetaHumanCharacterHeadModelSettings& InHeadModelSettings)
{
	Teeth = InHeadModelSettings.Teeth;
}

void UMetaHumanCharacterHeadModelTeethProperties::SetEnabled(bool bInIsEnabled)
{
	if (bInIsEnabled)
	{
		Teeth.EnableShowTeethExpression = true;
	}
	else
	{
		Teeth.EnableShowTeethExpression = false;
	}
}

void UMetaHumanCharacterEditorHeadModelTool::SetEnabledSubTool(UMetaHumanCharacterHeadModelSubToolBase* InSubTool, bool bInEnabled)
{
	if (InSubTool)
	{
		InSubTool->SetEnabled(bInEnabled);

		const bool bCommitChange = false;
		UpdateHeadModelState(bCommitChange);
	}
}

void UMetaHumanCharacterEditorHeadModelTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(GetDescription());

	EyelashesProperties = NewObject<UMetaHumanCharacterHeadModelEyelashesProperties>(this);
	EyelashesProperties->RestoreProperties(this);

	TeethProperties = NewObject<UMetaHumanCharacterHeadModelTeethProperties>(this);
	TeethProperties->RestoreProperties(this);

	RegisterSubTools();

	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	check(Subsystem);

	// Initialize the tool properties from the values stored in the Character
	FaceState = Subsystem->CopyFaceState(Character);
    PreviousHeadModelSettings = Character->HeadModelSettings;
	OriginalHeadModelSettings = PreviousHeadModelSettings;

	EyelashesProperties->CopyFrom(Character->HeadModelSettings);

	// Bind to the ValueSet event of the Head Model Properties to fill in the undo stack
	EyelashesProperties->OnEyelashesPropertyValueSetDelegate.BindWeakLambda(this, [this](bool bInIsEyelashesTypeModified)
		{
			if (UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target))
			{
				// Add finished changes in Eyelashes Properties to the undo stack
				FMetaHumanCharacterHeadModelSettings NewHeadModelSettings;
				EyelashesProperties->CopyTo(NewHeadModelSettings);
				
				// Add the undo command
				if (bInIsEyelashesTypeModified)
				{
					TUniquePtr<FMetaHumanCharacterEditorEyelashesTypeCommandChange> CommandChange =
						MakeUnique<FMetaHumanCharacterEditorEyelashesTypeCommandChange>(PreviousHeadModelSettings, NewHeadModelSettings, FaceState.ToSharedRef(), GetToolManager());
					GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("EyelashesTypeCommandChange", "Edit Eyelashes Type"));
				}
				else
				{
					TUniquePtr<FMetaHumanCharacterEditorHeadModelToolCommandChange> CommandChange =
						MakeUnique<FMetaHumanCharacterEditorHeadModelToolCommandChange>(PreviousHeadModelSettings, NewHeadModelSettings, GetToolManager());
					GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("HeadModelToolCommandChange", "Edit Head Model"));
				}

				PreviousHeadModelSettings = NewHeadModelSettings;
				bEyelashesVariantWasModified = true;

				const bool bCommitChange = false;
				UpdateHeadModelState(bCommitChange);
			}
		});

	TeethProperties->CopyFrom(Character->HeadModelSettings);

	// Bind to the ValueSet event of the Head Model Properties to fill in the undo stack
	TeethProperties->OnTeethPropertyValueSetDelegate.BindWeakLambda(this, [this](bool bInIsInteractive)
		{
			if (UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target))
			{
				bTeethVariantWasModified = true;
				if (!bInIsInteractive)
				{
					bTeethVariantWasCommitted = true;
				}
			}
		});

	// Updates the cached parameters of all property watchers to avoid triggering the update functions when the tool starts
	EyelashesProperties->SilentUpdateWatched();
	TeethProperties->SilentUpdateWatched();
}

void UMetaHumanCharacterEditorHeadModelTool::RegisterSubTools()
{
	const FMetaHumanCharacterEditorToolCommands& Commands = FMetaHumanCharacterEditorToolCommands::Get();
	SubTools->RegisterSubTools(
		{
			{ Commands.BeginHeadModelTeethTool, TeethProperties },
			{ Commands.BeginHeadModelEyelashesTool, EyelashesProperties }
		}, Commands.BeginHeadModelTeethTool);
}

const FText UMetaHumanCharacterEditorHeadModelTool::GetDescription() const
{
	return LOCTEXT("HeadModelToolName", "Model");
}

void UMetaHumanCharacterEditorHeadModelTool::Shutdown(EToolShutdownType InShutdownType)
{
	Super::Shutdown(InShutdownType);

	ProcessPending();

	EyelashesProperties->SaveProperties(this);

	// disable show teeth
	TeethProperties->Teeth.EnableShowTeethExpression = false;

	const bool bCommitChange = true;
	UpdateHeadModelState(bCommitChange);

	TeethProperties->SaveProperties(this);
}

void UMetaHumanCharacterEditorHeadModelTool::OnTick(float InDeltaTime)
{
	ProcessPending();
}

void UMetaHumanCharacterEditorHeadModelTool::ProcessPending()
{
	if (!bTeethVariantWasModified)
	{
		return;
	}

	if (UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target))
	{
		if (bTeethVariantWasCommitted && TeethProperties->Teeth != PreviousHeadModelSettings.Teeth)
		{
			FMetaHumanCharacterHeadModelSettings NewHeadModelSettings;
			TeethProperties->CopyTo(NewHeadModelSettings);
			TUniquePtr<FMetaHumanCharacterEditorHeadModelToolCommandChange> CommandChange =
				MakeUnique<FMetaHumanCharacterEditorHeadModelToolCommandChange>(PreviousHeadModelSettings, NewHeadModelSettings, GetToolManager());
			GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("HeadModelToolCommandChange", "Edit Head Model"));
			PreviousHeadModelSettings = NewHeadModelSettings;
		}
		bTeethVariantWasCommitted = false;
		bTeethVariantWasModified = false;

		const bool bCommitChange = false;
		UpdateHeadModelState(bCommitChange);
	}
}

void UMetaHumanCharacterEditorHeadModelTool::UpdateHeadModelState(bool bInCommitChange) const
{
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	check(Subsystem);

	FMetaHumanCharacterHeadModelSettings NewSettings;
	EyelashesProperties->CopyTo(NewSettings);
	TeethProperties->CopyTo(NewSettings);

	if (bInCommitChange)
	{
		Subsystem->CommitHeadModelSettings(Character, NewSettings);
	}
	else
	{
		Subsystem->ApplyHeadModelSettings(Character, NewSettings);
	}
}

void UMetaHumanCharacterEditorHeadMaterialsTool::Setup()
{
	Super::Setup();

	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	check(Subsystem);

	// Auto select skin preview
	if (Character->PreviewMaterialType != EMetaHumanCharacterSkinPreviewMaterial::Editable)
	{
		Subsystem->UpdateCharacterPreviewMaterial(Character, EMetaHumanCharacterSkinPreviewMaterial::Editable);
	}
}

void UMetaHumanCharacterEditorHeadMaterialsTool::RegisterSubTools()
{
	const FMetaHumanCharacterEditorToolCommands& Commands = FMetaHumanCharacterEditorToolCommands::Get();
	SubTools->RegisterSubTools(
		{
			{ Commands.BeginHeadMaterialsEyelashesTool, EyelashesProperties },
			{ Commands.BeginHeadMaterialsTeethTool, TeethProperties },
		}, Commands.BeginHeadMaterialsTeethTool);
}

const FText UMetaHumanCharacterEditorHeadMaterialsTool::GetDescription() const
{
	return LOCTEXT("HeadMaterialsToolName", "Head Model");
}

void UMetaHumanCharacterEditorHeadMaterialsTool::Shutdown(EToolShutdownType InShutdownType)
{
	Super::Shutdown(InShutdownType);
}

void UMetaHumanCharacterEditorHeadMaterialsTool::OnTick(float InDeltaTime)
{
	Super::OnTick(InDeltaTime);
}

#undef LOCTEXT_NAMESPACE
