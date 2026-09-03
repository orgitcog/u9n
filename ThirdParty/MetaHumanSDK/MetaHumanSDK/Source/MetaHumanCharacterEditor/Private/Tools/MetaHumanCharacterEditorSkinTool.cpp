// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorSkinTool.h"
#include "MetaHumanCharacterEditorActor.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterEditorToolCommandChange.h"
#include "MetaHumanFaceTextureSynthesizer.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "ToolBuilderUtil.h"
#include "ToolTargetManager.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolChange.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "Editor/EditorEngine.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditorSkinTool"

// Undo command for keeping track of changes in the Character skin settings
class FMetaHumanCharacterEditorSkinToolCommandChange : public FToolCommandChange
{
public:
	FMetaHumanCharacterEditorSkinToolCommandChange(const FMetaHumanCharacterSkinSettings& InOldSkinSettings,
												   const FMetaHumanCharacterSkinSettings& InNewSkinSettings,
												   TNotNull<UInteractiveToolManager*> InToolManager)
		: OldSkinSettings(InOldSkinSettings)
		, NewSkinSettings(InNewSkinSettings)
		, ToolManager{ InToolManager }
	{
	}

	//~Begin FToolCommandChange interface
	virtual FString ToString() const override
	{
		return TEXT("MetaHuman Character Edit Skin");
	}

	virtual bool HasExpired(UObject* InObject) const override
	{
		// If the ToolManager is not valid anymore it means the asset editor was closed so mark the transaction as expired
		return !ToolManager.IsValid();
	}

	void Apply(UObject* InObject) override
	{
		UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacter>(InObject);
		UMetaHumanCharacterEditorSubsystem::Get()->CommitSkinSettings(MetaHumanCharacter, NewSkinSettings);

		UpdateSkinToolProperties(NewSkinSettings);
	}

	void Revert(UObject* InObject) override
	{
		UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacter>(InObject);
		UMetaHumanCharacterEditorSubsystem::Get()->CommitSkinSettings(MetaHumanCharacter, OldSkinSettings);

		UpdateSkinToolProperties(OldSkinSettings);
	}
	//~End FToolCommandChange interface

protected:

	/**
	 * Updates the Skin Tool Properties of the active tool using the given skin settings
	 */
	void UpdateSkinToolProperties(const FMetaHumanCharacterSkinSettings& InSkinSettings)
	{
		if (ToolManager.IsValid())
		{
			if (UMetaHumanCharacterEditorSkinTool* SkinTool = Cast<UMetaHumanCharacterEditorSkinTool>(ToolManager->GetActiveTool(EToolSide::Left)))
			{
				UMetaHumanCharacterEditorSkinToolProperties* SkinToolProperties = nullptr;
				if (SkinTool->GetToolProperties().FindItemByClass<UMetaHumanCharacterEditorSkinToolProperties>(&SkinToolProperties))
				{
					SkinToolProperties->CopyFrom(InSkinSettings);
					SkinToolProperties->SilentUpdateWatched();

					// Restore the PreviousSkinSettings of the tool to what we are applying so that
					// new commands are created with the correct previous settings
					SkinTool->PreviousSkinSettings = InSkinSettings;
				}
			}
		}
	}

protected:

	// Store as FMetaHumanCharacterSkinSettings since it is simpler to manage the lifetime of structs
	FMetaHumanCharacterSkinSettings OldSkinSettings;
	FMetaHumanCharacterSkinSettings NewSkinSettings;

	// Reference to skin tool manager, used to update the skin tool properties when applying transactions
	TWeakObjectPtr<UInteractiveToolManager> ToolManager;
};

// Specialized version of the the skin edit command that also updates the face state HF variant
class FMetaHumanCharacterEditorSkinTextureCommandChange : public FMetaHumanCharacterEditorSkinToolCommandChange
{
public:
	FMetaHumanCharacterEditorSkinTextureCommandChange(const FMetaHumanCharacterSkinSettings& InOldSkinSettings,
													  const FMetaHumanCharacterSkinSettings& InNewSkinSettings,
													  TSharedRef<FMetaHumanCharacterIdentity::FState> InReferenceFaceState,
													  TNotNull<UInteractiveToolManager*> InToolManager)
		: FMetaHumanCharacterEditorSkinToolCommandChange(InOldSkinSettings, InNewSkinSettings, InToolManager)
		, ReferenceFaceState(InReferenceFaceState)
	{
	}

	//~Begin FToolCommandChange interface
	virtual FString ToString() const override
	{
		return TEXT("MetaHuman Character Edit Skin Texture");
	}

	void Apply(UObject* InObject) override
	{
		ApplySkinSettingsAndHFVariant(InObject, NewSkinSettings);

	}

	void Revert(UObject* InObject) override
	{
		ApplySkinSettingsAndHFVariant(InObject, OldSkinSettings);
	}
	//~End FToolCommandChange interface

private:
	// State to be used for applying the HF variant from the Texture skin property
	TSharedRef<FMetaHumanCharacterIdentity::FState> ReferenceFaceState;


	void ApplySkinSettingsAndHFVariant(UObject* InObject, const FMetaHumanCharacterSkinSettings& InSkinSettings)
	{
		UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacter>(InObject);
		UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();

		MetaHumanCharacterSubsystem->CommitSkinSettings(MetaHumanCharacter, InSkinSettings);

		// Copy the reference state and apply the HF variant
		TSharedRef<FMetaHumanCharacterIdentity::FState> NewState = MakeShared<FMetaHumanCharacterIdentity::FState>(*ReferenceFaceState);
		MetaHumanCharacterSubsystem->UpdateHFVariantFromSkinProperties(NewState, InSkinSettings.Skin);
		MetaHumanCharacterSubsystem->CommitFaceState(MetaHumanCharacter, NewState);

		UpdateSkinToolProperties(InSkinSettings);
	}
};

UInteractiveTool* UMetaHumanCharacterEditorSkinToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	UMetaHumanCharacterEditorSkinTool* SkinTool = NewObject<UMetaHumanCharacterEditorSkinTool>(InSceneState.ToolManager);
	SkinTool->SetTarget(Target);

	return SkinTool;
}

bool UMetaHumanCharacterEditorSkinToolBuilder::CanBuildTool(const FToolBuilderState& InSceneState) const
{
	bool bCanBuildTool = Super::CanBuildTool(InSceneState);

	UActorComponent* Component = ToolBuilderUtil::FindFirstComponent(InSceneState, [&](UActorComponent* Component)
	{
		return IsValid(Component) && Component->GetOwner()->Implements<UMetaHumanCharacterEditorActorInterface>();;
	});

	if (Component)
	{
		IMetaHumanCharacterEditorActorInterface* CharacterActorInterface = Cast<IMetaHumanCharacterEditorActorInterface>(Component->GetOwner());
		bool bIsRequestingHighResTextures = UMetaHumanCharacterEditorSubsystem::Get()->IsRequestingHighResolutionTextures(CharacterActorInterface->GetCharacter());
		bCanBuildTool = bCanBuildTool && !bIsRequestingHighResTextures;
	}

	return bCanBuildTool;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorSkinToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass(),
		}
	);

	return TypeRequirements;
}

void UMetaHumanCharacterEditorSkinToolProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Override function to process EPropertyChangeType::ValueSet events for the edited properties
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnSkinPropertyValueSetDelegate.ExecuteIfBound(PropertyChangedEvent);
}

bool UMetaHumanCharacterEditorSkinToolProperties::CanEditChange(const FProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);

	if (bIsEditable && InProperty != nullptr)
	{
		const FName PropertyName = InProperty->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSkinToolProperties, Skin))
		{
			UMetaHumanCharacterEditorSkinTool* SkinTool = GetTypedOuter<UMetaHumanCharacterEditorSkinTool>();
			check(SkinTool);

			UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(SkinTool->GetTarget());
			check(Character);

			const bool bIsRequestingTextures = UMetaHumanCharacterEditorSubsystem::Get()->IsRequestingHighResolutionTextures(Character);
			bIsEditable = !bIsRequestingTextures;
		}
	}

	return bIsEditable;
}

void UMetaHumanCharacterEditorSkinToolProperties::CopyTo(FMetaHumanCharacterSkinSettings& OutSkinSettings)
{
	OutSkinSettings.Skin = Skin;
	OutSkinSettings.Freckles = Freckles;
	OutSkinSettings.Accents = Accents;
	OutSkinSettings.bEnableTextureOverrides = bEnableTextureOverrides;
	OutSkinSettings.TextureOverrides = TextureOverrides;
}

void UMetaHumanCharacterEditorSkinToolProperties::CopyFrom(const FMetaHumanCharacterSkinSettings& InSkinSettings)
{
	Skin = InSkinSettings.Skin;
	Freckles = InSkinSettings.Freckles;
	Accents = InSkinSettings.Accents;
	bEnableTextureOverrides = InSkinSettings.bEnableTextureOverrides;
	TextureOverrides = InSkinSettings.TextureOverrides;
}

void UMetaHumanCharacterEditorSkinToolProperties::CopyTo(FMetaHumanCharacterFaceEvaluationSettings& OutFaceEvaluationSettings)
{
	OutFaceEvaluationSettings = FaceEvaluationSettings;
}

void UMetaHumanCharacterEditorSkinToolProperties::CopyFrom(const FMetaHumanCharacterFaceEvaluationSettings& InFaceEvaluationSettings)
{
	FaceEvaluationSettings = InFaceEvaluationSettings;
}

void UMetaHumanCharacterEditorSkinTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(LOCTEXT("SkinToolName", "Skin"));

	SkinToolProperties = NewObject<UMetaHumanCharacterEditorSkinToolProperties>(this);
	AddToolPropertySource(SkinToolProperties);

	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();

	// Initialize the tool properties from the values stored in the Character
	FaceState = Subsystem->CopyFaceState(Character);
	PreviousSkinSettings = Character->SkinSettings;
	PreviousFaceEvaluationSettings = Character->FaceEvaluationSettings;

	SkinToolProperties->CopyFrom(Character->SkinSettings);
	SkinToolProperties->CopyFrom(Character->FaceEvaluationSettings);
	FilteredFaceTextureIndices.Reset();

	SkinToolProperties->SkinFilterValues.Reset();
	int32 NumTextureAttributes = Subsystem->GetFaceTextureAttributeMap().NumAttributes();
	for (int32 Idx = 0; Idx < NumTextureAttributes; ++Idx)
	{
		SkinToolProperties->SkinFilterValues.Push(int32(-1));
	}

	// Bind to the ValueSet event of the Skin Properties to fill in the undo stack
	SkinToolProperties->OnSkinPropertyValueSetDelegate.BindWeakLambda(this, [this](const FPropertyChangedEvent& PropertyChangedEvent)
		{
			if (UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target))
			{
				const FName PropertyName = PropertyChangedEvent.GetPropertyName();

				UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();

				if (PropertyName == GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFaceEvaluationSettings, HighFrequencyDelta))
				{
					// update the face settings only if they differ
					FMetaHumanCharacterFaceEvaluationSettings NewFaceEvaluationSettings;
					SkinToolProperties->CopyTo(NewFaceEvaluationSettings);

					if (Character->FaceEvaluationSettings == NewFaceEvaluationSettings)
					{
						return;
					}
					if ((PropertyChangedEvent.ChangeType & (EPropertyChangeType::ValueSet | EPropertyChangeType::ResetToDefault)) != 0u && ((PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive) == 0u))
					{
						Subsystem->CommitFaceEvaluationSettings(Character, NewFaceEvaluationSettings);

						FOnSettingsUpdateDelegate OnSettingsUpdateDelegate;
						OnSettingsUpdateDelegate.BindWeakLambda(this, [this](TWeakObjectPtr<UInteractiveToolManager> ToolManager, const FMetaHumanCharacterFaceEvaluationSettings& FaceEvaluationSettings)
							{
								UpdateSkinToolProperties(ToolManager, FaceEvaluationSettings);
							});

						TUniquePtr<FMetaHumanCharacterEditorFaceEvaluationSettingsCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorFaceEvaluationSettingsCommandChange>(Character, PreviousFaceEvaluationSettings, OnSettingsUpdateDelegate, GetToolManager());
						GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("SkinToolVertexDeltaCommandChange", "Face Blend Tool Vertex Delta"));
						PreviousFaceEvaluationSettings = NewFaceEvaluationSettings;
					}
					else
					{
						Subsystem->ApplyFaceEvaluationSettings(Character, NewFaceEvaluationSettings);
					}
				}
				else
				{
					bool bIsSkinModified = false;
					bool bIsTextureModified = false;
					// When the reset to default button is clicked in the details panel ChangeType will have both ValueSet and ResetToDefault bits set
					if ((PropertyChangedEvent.ChangeType & (EPropertyChangeType::ValueSet | EPropertyChangeType::ResetToDefault)) != 0u)
					{
						bIsSkinModified = true;
						// The Skin Texture property is handled differently since we need to update both texture and face state
						if (PropertyName == GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, FaceTextureIndex) ||
							PropertyName == GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, BodyTextureIndex))
						{
							bIsTextureModified = true;
						}
					}
					else
					{
						// The Skin Texture property is handled differently since we need to update both texture and face state
						if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSkinToolProperties, TextureOverrides))
						{
							bIsSkinModified = true;
						}

						// Mark the skin as modified if accent regions or freckles have changed
						if (PropertyChangedEvent.Property->GetOwnerStruct() == FMetaHumanCharacterAccentRegionProperties::StaticStruct() ||
							PropertyChangedEvent.Property->GetOwnerStruct() == FMetaHumanCharacterFrecklesProperties::StaticStruct())
						{
							bIsSkinModified = true;
						}
					}

					if (bIsSkinModified)
					{
						// Add finished changes in Skin Properties to the undo stack
						FMetaHumanCharacterSkinSettings NewSkinSettings;
						SkinToolProperties->CopyTo(NewSkinSettings);

						// Add the undo command
						if (bIsTextureModified)
						{
							TUniquePtr<FMetaHumanCharacterEditorSkinTextureCommandChange> CommandChange =
								MakeUnique<FMetaHumanCharacterEditorSkinTextureCommandChange>(PreviousSkinSettings, NewSkinSettings, FaceState.ToSharedRef(), GetToolManager());
							GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("SkinTextureCommandChange", "Edit Skin Texture"));
							bSkinTextureWasModified = true;
						}
						else
						{
							TUniquePtr<FMetaHumanCharacterEditorSkinToolCommandChange> CommandChange =
								MakeUnique<FMetaHumanCharacterEditorSkinToolCommandChange>(PreviousSkinSettings, NewSkinSettings, GetToolManager());
							GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("SkinToolCommandChange", "Edit Skin"));
						}

						PreviousSkinSettings = NewSkinSettings;
						bActorWasModified = true;

						UpdateSkinState();
					}
				}
			}
		});

	SkinToolProperties->WatchProperty(SkinToolProperties->Skin.U,
		[this](float)
		{
			UpdateSkinSynthesizedTexture();
		});

	SkinToolProperties->WatchProperty(SkinToolProperties->Skin.V,
		[this](float)
		{
			UpdateSkinSynthesizedTexture();
		});

	SkinToolProperties->WatchProperty(SkinToolProperties->Skin.FaceTextureIndex,
		[this](int32)
		{
			UpdateSkinSynthesizedTexture();
		});

	SkinToolProperties->WatchProperty(SkinToolProperties->SkinFilterIndex,
		[this](int32)
		{
			UpdateFaceTextureFromFilterIndex();
		});

	SkinToolProperties->WatchProperty(SkinToolProperties->bIsSkinFilterEnabled,
		[this](bool bInIsSkinFilterEnabled)
		{
			SetEnableSkinFilter(bInIsSkinFilterEnabled);
		});

	SkinToolProperties->WatchProperty(SkinToolProperties->SkinFilterValues,
		[this](const TArray<int32>& Values)
		{
			SetEnableSkinFilter(SkinToolProperties->bIsSkinFilterEnabled);
		});

	SkinToolProperties->WatchProperty(SkinToolProperties->Skin.BodyTextureIndex,
		[this](int32)
		{
			UpdateSkinSynthesizedTexture();
		});

	SkinToolProperties->WatchProperty(SkinToolProperties->Skin.Roughness,
									  [this](float)
									  {
										UpdateSkinState();
									  });

	// Update the max values of the face texture slider based on the texture model
	FProperty* FaceTextureIndexProperty = FMetaHumanCharacterSkinProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, FaceTextureIndex));
	FaceTextureIndexProperty->SetMetaData("UIMax", FString::FromInt(Subsystem->GetMaxHighFrequencyIndex() - 1));
	FaceTextureIndexProperty->SetMetaData("ClampMax", FString::FromInt(Subsystem->GetMaxHighFrequencyIndex() - 1));

	// Updates the cached parameters of all property watchers to avoid triggering the update functions when the tool starts
	SkinToolProperties->SilentUpdateWatched();

	// Auto select skin preview if in topology mode
	if (Character->PreviewMaterialType == EMetaHumanCharacterSkinPreviewMaterial::Default)
	{
		Subsystem->UpdateCharacterPreviewMaterial(Character, EMetaHumanCharacterSkinPreviewMaterial::Editable);
	}
}

void UMetaHumanCharacterEditorSkinTool::Shutdown(EToolShutdownType InShutdownType)
{
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();

	if (bActorWasModified)
	{
		FMetaHumanCharacterSkinSettings CurrentSkinSettings;
		SkinToolProperties->CopyTo(CurrentSkinSettings);

		Subsystem->CommitSkinSettings(Character, CurrentSkinSettings);
		if (bSkinTextureWasModified)
		{
			Subsystem->CommitFaceState(Character, Subsystem->GetFaceState(Character));
		}

		// Add the undo command
		const FText CommandChangeDescription = FText::Format(LOCTEXT("SkinEditingCommandChangeTransaction", "{0} {1}"),
			UEnum::GetDisplayValueAsText(InShutdownType),
			GetCommandChangeDescription());

		// OriginalSkinSettings were either set when
		// - tool opened (Cancel)
		// - in the statement above to the latest settings (Accept)
		// in both cases we add a command from PreviousSkinSettings -> OriginalSkinSettings
		if (bSkinTextureWasModified)
		{
			TUniquePtr<FMetaHumanCharacterEditorSkinTextureCommandChange> CommandChange =
				MakeUnique<FMetaHumanCharacterEditorSkinTextureCommandChange>(PreviousSkinSettings, CurrentSkinSettings, FaceState.ToSharedRef(), GetToolManager());
			GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), CommandChangeDescription);
		}
		else
		{
			TUniquePtr<FMetaHumanCharacterEditorSkinToolCommandChange> CommandChange =
				MakeUnique<FMetaHumanCharacterEditorSkinToolCommandChange>(PreviousSkinSettings, CurrentSkinSettings, GetToolManager());
			GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character,
				MoveTemp(CommandChange), CommandChangeDescription);
		}
	}
}

void UMetaHumanCharacterEditorSkinTool::UpdateSkinState() const
{
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	FMetaHumanCharacterSkinSettings NewSettings;
	SkinToolProperties->CopyTo(NewSettings);

	UMetaHumanCharacterEditorSubsystem::Get()->ApplySkinSettings(Character, NewSettings);
}

const FText UMetaHumanCharacterEditorSkinTool::GetCommandChangeDescription() const
{
	return LOCTEXT("FaceSkinToolCommandChange", "Face Skin Tool");
}

bool UMetaHumanCharacterEditorSkinTool::UpdateSkinSynthesizedTexture()
{
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(MetaHumanCharacter);

	bool bCanUpdate = true;

	if (MetaHumanCharacter->HasHighResolutionTextures())
	{
		const FText Message = LOCTEXT("PromptHighResTexture", "This MetaHuman has high resolution textures assigned to it, making this change will discard the current texture and replace it with a lower resolution one. Do you want to continue?");

		const EAppReturnType::Type Reply = FMessageDialog::Open(EAppMsgType::YesNo, Message);
		bCanUpdate = (Reply == EAppReturnType::Yes);
	}

	if (bCanUpdate)
	{
		const bool bHadHighResolutionTextures = MetaHumanCharacter->HasHighResolutionTextures();

		UpdateSkinState();

		if (bHadHighResolutionTextures)
		{
			// If we can update but the character had high resolution textures before the update, it means a dialog asking the user
			// to proceed was displayed. In this case, for some reason, the ValueSet event is not emitted so we are emitting one
			// here to make sure the skin tool registers the change and creates a transaction for it
			const FName SkinPropertyName = GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSkinToolProperties, Skin);
			FProperty* SkinProperty = UMetaHumanCharacterEditorSkinToolProperties::StaticClass()->FindPropertyByName(SkinPropertyName);
			FPropertyChangedEvent ValueSetEvent{ SkinProperty, EPropertyChangeType::ValueSet };
			SkinToolProperties->PostEditChangeProperty(ValueSetEvent);
		}
	}
	else
	{
		// Restore the previous skin texture parameters
		SkinToolProperties->Skin = PreviousSkinSettings.Skin;
		SkinToolProperties->SilentUpdateWatched();
	}


	return bCanUpdate;
}

void UMetaHumanCharacterEditorSkinTool::UpdateSkinToolProperties(TWeakObjectPtr<UInteractiveToolManager> InToolManager, const FMetaHumanCharacterFaceEvaluationSettings& InFaceEvaluationSettings)
{
	if (InToolManager.IsValid())
	{
		SkinToolProperties->CopyFrom(InFaceEvaluationSettings);
		SkinToolProperties->SilentUpdateWatched();

		// Restore the PreviousSkinSettings of the tool to what we are applying so that
		// new commands are created with the correct previous settings
		PreviousFaceEvaluationSettings = InFaceEvaluationSettings;
	}
}

void UMetaHumanCharacterEditorSkinTool::UpdateFaceTextureFromFilterIndex()
{
	if (FilteredFaceTextureIndices.IsValid())
	{
		const int32 FaceTextureIndex = FilteredFaceTextureIndices->ConvertFilterIndexToTextureIndex(SkinToolProperties->SkinFilterIndex);

		if (FaceTextureIndex >= 0 && FaceTextureIndex < UMetaHumanCharacterEditorSubsystem::Get()->GetMaxHighFrequencyIndex())
		{
			SkinToolProperties->Skin.FaceTextureIndex = FaceTextureIndex;
		}
	}
}

void UMetaHumanCharacterEditorSkinTool::SetEnableSkinFilter(bool bInEnableSkinFilter)
{
	if (bInEnableSkinFilter)
	{
		UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
		
		FilteredFaceTextureIndices = MakeShared<FMetaHumanFilteredFaceTextureIndices>(Subsystem->GetFaceTextureAttributeMap(), SkinToolProperties->SkinFilterValues);
		SkinToolProperties->SkinFilterIndex = FilteredFaceTextureIndices->ConvertTextureIndexToFilterIndex(SkinToolProperties->Skin.FaceTextureIndex);

		// Update the max values of the skin filter slider
		FProperty* SkinFilterIndexProperty = UMetaHumanCharacterEditorSkinToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSkinToolProperties, SkinFilterIndex));
		SkinFilterIndexProperty->SetMetaData("UIMax", FString::FromInt(FilteredFaceTextureIndices->Num() - 1));
		SkinFilterIndexProperty->SetMetaData("ClampMax", FString::FromInt(FilteredFaceTextureIndices->Num() - 1));
	}
	else
	{
		FilteredFaceTextureIndices.Reset();
	}
}

bool UMetaHumanCharacterEditorSkinTool::IsFilteredFaceTextureIndicesValid() const
{
	return (FilteredFaceTextureIndices.IsValid() && FilteredFaceTextureIndices->Num() > 0);
}

#undef LOCTEXT_NAMESPACE