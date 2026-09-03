// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterMigrationEditorModule.h"
#include "MetaHumanCharacterFactoryNew.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterEditorModule.h"
#include "MetaHumanCharacterPaletteEditorModule.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanWardrobeItem.h"
#include "Item/MetaHumanDefaultGroomPipeline.h"
#include "MetaHumanMigrationInfo.h"
#include "MetaHumanMigrationDatabase.h"
#include "Import/MetaHumanImport.h"
#include "MetaHumanTypesEditor.h"
#include "MetaHumanCharacterEditorSettings.h"
#include "Dialog/SCustomDialog.h"
#include "JsonObjectConverter.h"
#include "IAssetTools.h"
#include "DNAUtils.h"
#include "GroomBindingAsset.h"
#include "Widgets/Text/STextBlock.h"
#include "Logging/MessageLog.h"
#include "Editor/EditorEngine.h"
#include "Misc/FileHelper.h"
#include "Util/ColorConstants.h"
#include "AssetRegistry/IAssetRegistry.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterMigrationEditorModule"

namespace UE::MetaHuman::Private
{
	TNotNull<const UMetaHumanMigrationDatabase*> GetMigrationDatabase()
	{
		FSoftObjectPath MigrationDatabaseAssetPath(TEXT("/" UE_PLUGIN_NAME "/Optional/Migration/MigrationDatabase.MigrationDatabase"));
		const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(MigrationDatabaseAssetPath, true);
		check(AssetData.IsValid());
		
		return Cast<UMetaHumanMigrationDatabase>(AssetData.GetAsset());
	}

	template<typename T>
	void SetPropertyBagValue(FInstancedPropertyBag& InOutPropertyBag, const FName& InPropertyName, T InValue)
	{
		static_assert(sizeof(T) == 0, "Unsupported type");
	}

	template<>
	void SetPropertyBagValue<bool>(FInstancedPropertyBag& InOutPropertyBag, const FName& InPropertyName, bool bInValue)
	{
		InOutPropertyBag.AddProperty(InPropertyName, EPropertyBagPropertyType::Bool);
		InOutPropertyBag.SetValueBool(InPropertyName, bInValue);
	}

	template<>
	void SetPropertyBagValue<float>(FInstancedPropertyBag& InOutPropertyBag, const FName& InPropertyName, float InValue)
	{
		InOutPropertyBag.AddProperty(InPropertyName, EPropertyBagPropertyType::Float);
		InOutPropertyBag.SetValueFloat(InPropertyName, InValue);
	}

	template<>
	void SetPropertyBagValue<UObject*>(FInstancedPropertyBag& InOutPropertyBag, const FName& InPropertyName, UObject* InValue)
	{
		InOutPropertyBag.AddProperty(InPropertyName, EPropertyBagPropertyType::Object);
		InOutPropertyBag.SetValueObject(InPropertyName, InValue);
	}

	template<typename T>
	void SetPropertyBagValueStruct(FInstancedPropertyBag& InOutPropertyBag, const FName& InPropertyName, FConstStructView InValue)
	{
		FPropertyBagPropertyDesc PropertyDesc(InPropertyName, EPropertyBagPropertyType::Struct, TBaseStructure<T>::Get());
		InOutPropertyBag.AddProperties({ PropertyDesc });
		InOutPropertyBag.SetValueStruct(InPropertyName, InValue);
	}

	bool TryGetGroomMigrationInfo(const FMetaHumanMigrationInfo& MigrationInfo, EMetaHumanMigrationDataAssetType InType, FMetaHumanGroomMigrationInfo& OutInfo)
	{
		switch (InType)
		{
		case EMetaHumanMigrationDataAssetType::Hair:
			OutInfo = MigrationInfo.Grooms.Hair;
			return true;
		case EMetaHumanMigrationDataAssetType::Eyebrows:
			OutInfo = MigrationInfo.Grooms.Eyebrows;
			return true;
		case EMetaHumanMigrationDataAssetType::Eyelashes:
			OutInfo = MigrationInfo.Grooms.Eyelashes;
			return true;
		case EMetaHumanMigrationDataAssetType::Beard:
			OutInfo = MigrationInfo.Grooms.Beard;
			return true;
		case EMetaHumanMigrationDataAssetType::Mustache:
			OutInfo = MigrationInfo.Grooms.Mustache;
			return true;
		case EMetaHumanMigrationDataAssetType::Peachfuzz:
			OutInfo = MigrationInfo.Grooms.Peachfuzz;
			return true;
		default:
			return false;
		};
	};

}

void FMetaHumanCharacterMigrationEditorModule::StartupModule()
{
	using namespace UE::MetaHuman;

	FMetaHumanImport::Get()->OnImportStartedDelegate.BindRaw(this, &FMetaHumanCharacterMigrationEditorModule::OnMetaHumanImportStarted);
	FMetaHumanImport::Get()->OnShouldImportAssetOrFileDelegate.BindRaw(this, &FMetaHumanCharacterMigrationEditorModule::OnShouldImportMetaHumanAssetOrFile);
}

void FMetaHumanCharacterMigrationEditorModule::ShutdownModule()
{
	using namespace UE::MetaHuman;

	FMetaHumanImport::Get()->OnImportStartedDelegate.Unbind();
	FMetaHumanImport::Get()->OnShouldImportAssetOrFileDelegate.Unbind();
	FMetaHumanImport::Get()->OnImportEndedDelegate.Unbind();
}

bool FMetaHumanCharacterMigrationEditorModule::ShouldMigrate() const
{
	return MigrateActionInternal == EMetaHumanCharacterMigrationAction::Migrate ||
		MigrateActionInternal == EMetaHumanCharacterMigrationAction::ImportAndMigrate;
}

bool FMetaHumanCharacterMigrationEditorModule::ShouldImport() const
{
	return MigrateActionInternal == EMetaHumanCharacterMigrationAction::Import ||
		MigrateActionInternal == EMetaHumanCharacterMigrationAction::ImportAndMigrate;
}

bool FMetaHumanCharacterMigrationEditorModule::OnMetaHumanImportStarted(const UE::MetaHuman::FSourceMetaHuman& InSourceMetaHuman)
{
	bHasWarnings = false;
	bHasErrors = false;

	// If we don't have a migration file, then import is the only option. Skip the dialog
	const FString MigrationInfoJsonFilePath = InSourceMetaHuman.GetSourceAssetsPath() / TEXT("MigrationInfo.json");
	if (!FPaths::FileExists(MigrationInfoJsonFilePath))
	{
		MigrateActionInternal = EMetaHumanCharacterMigrationAction::Import;
		return true;
	}

	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();

	if (Settings->MigrationAction == EMetaHumanCharacterMigrationAction::Prompt)
	{
		TSharedRef<SCustomDialog> MigrateActionDialog =
			SNew(SCustomDialog)
			.Title(LOCTEXT("MigrationDialogTitle", "Migration Action"))
			.Content()
			[
				SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					[
						SNew(STextBlock)
							.Text(FText::Format(LOCTEXT("MigrationDialogMessage", "Which action to perform for MetaHuman '{0}'"), { FText::FromString(InSourceMetaHuman.GetName()) }))
					]
					+ SVerticalBox::Slot()
					[
						SNew(STextBlock)
							.Text(LOCTEXT("MigrationMessageProjectSettings", "The default action can be set in the Project Settings"))
					]
			]
			.Buttons(
				{
					SCustomDialog::FButton(LOCTEXT("MigrationActionImport", "Import")),
					SCustomDialog::FButton(LOCTEXT("MigrationActionMigrate", "Migrate")),
					SCustomDialog::FButton(LOCTEXT("MigrationActionImportAndMigrate", "Import and Migrate")),
				}
			);

		const int32 ActionChoice = MigrateActionDialog->ShowModal();
		switch (ActionChoice)
		{
			case 0:
				MigrateActionInternal = EMetaHumanCharacterMigrationAction::Import;
				break;

			case 1:
				MigrateActionInternal = EMetaHumanCharacterMigrationAction::Migrate;
				break;

			case 2:
				MigrateActionInternal = EMetaHumanCharacterMigrationAction::ImportAndMigrate;
				break;

			case -1:
			default:
				return false;
		}
	}
	else
	{
		MigrateActionInternal = Settings->MigrationAction;
	}

	if (ShouldMigrate())
	{
		MigrateMetaHuman(InSourceMetaHuman);
	}

	return ShouldImport();
}

bool FMetaHumanCharacterMigrationEditorModule::OnShouldImportMetaHumanAssetOrFile(const UE::MetaHuman::FSourceMetaHuman& InSourceMetaHuman, const FString& InDestPath, bool bInIsFile)
{
	if (bInIsFile)
	{
		if (FPaths::GetCleanFilename(InDestPath) == TEXT("MigrationInfo.json"))
		{
			// If importing, skip MigrationInfo.json as it has no use within a project
			return false;
		}
	}

	// Right now there no need to need to import anything to perform the migration step but this function can be used to
	// bring legacy MetaHuman assets to the project if needed
	return ShouldImport();
}

void FMetaHumanCharacterMigrationEditorModule::MigrateMetaHuman(const UE::MetaHuman::FSourceMetaHuman& InSourceMetaHuman)
{
	FMessageLog MessageLog{ UE::MetaHuman::MessageLogName };
	MessageLogPtr = &MessageLog;

	MessageLog.Info(FText::Format(LOCTEXT("MigrationStarted", "Started Migrating '{0}'"), FText::FromString(InSourceMetaHuman.GetName())));

	FString CreatedPackageName;

	ON_SCOPE_EXIT
	{
		if (bHasErrors)
		{
			MessageLog.Error(FText::Format(LOCTEXT("MigrationError", "Error migrating '{0}'"), FText::FromString(InSourceMetaHuman.GetName())));
		}
		else if (bHasWarnings)
		{
			MessageLog.Warning(LOCTEXT("MigrationWarnings", "MetaHuman migration completed with warnings: "))
				->AddToken(FAssetNameToken::Create(CreatedPackageName));
		}
		else
		{
			MessageLog.Info(LOCTEXT("MigrationSuccesfull", "MetaHuman succesfully migrated: "))
				->AddToken(FAssetNameToken::Create(CreatedPackageName));
		}

		const bool bOpenIfEmpty = true;
		MessageLog.Open(EMessageSeverity::Info, bOpenIfEmpty);

		MessageLogPtr = nullptr;
	};

	// Check to see if the MigrationInfo.json and DNA files are available
	const FString MigrationInfoJsonFilePath = InSourceMetaHuman.GetSourceAssetsPath() / TEXT("MigrationInfo.json");
	const FString DNAFilepath = InSourceMetaHuman.GetSourceAssetsPath() / InSourceMetaHuman.GetName() + TEXT(".dna");

	if (!FPaths::FileExists(MigrationInfoJsonFilePath))
	{
		LogError(FText::Format(LOCTEXT("MigrationInfoJsonNotFound", "MigrationInfo.json not found. Please make sure '{0}' was updated to the latest version"), FText::FromString(InSourceMetaHuman.GetName())));
		return;
	}

	if (!FPaths::FileExists(DNAFilepath))
	{
		LogError(LOCTEXT("DNANotFound", "DNA file not found"));
		return;
	}

	if (!FMetaHumanCharacterEditorModule::IsOptionalMetaHumanContentInstalled())
	{
		LogError(LOCTEXT("OptionalContentNotInstalled", "MetaHuman content is not installed, migration cannot continue. Please download the needed content are re-run the migration."));
		return;
	}

	FScopedSlowTask MigrateTask(8.0f, FText::Format(LOCTEXT("MigrateTaskMessage", "Migrating MetaHuman {0}"), FText::FromString(InSourceMetaHuman.GetName())));
	MigrateTask.MakeDialog();

	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();

	const FString MigratedCharacterPackagePath = Settings->MigratedPackagePath.Path;
	const FString MigratedCharacterNamePrefix = Settings->MigratedNamePrefix;
	const FString MigratedCharacterNameSuffix = Settings->MigratedNameSuffix;

	IAssetTools& AssetTools = IAssetTools::Get();

	const FString CandidateName = FString::Format(TEXT("{0}/{1}{2}"), { MigratedCharacterPackagePath, MigratedCharacterNamePrefix, InSourceMetaHuman.GetName() });
	FString NewCharacterAssetPackageName;
	FString NewCharacterAssetName;
	AssetTools.CreateUniqueAssetName(CandidateName, MigratedCharacterNameSuffix, NewCharacterAssetPackageName, NewCharacterAssetName);

	MigrateTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("CreatingCharacterMessage", "Creating MetaHuman {0}"), FText::FromString(NewCharacterAssetName)));

	UMetaHumanCharacterFactoryNew* Factory = NewObject<UMetaHumanCharacterFactoryNew>();
	UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(AssetTools.CreateAsset(NewCharacterAssetName,
																	  FPackageName::GetLongPackagePath(NewCharacterAssetPackageName),
																	  UMetaHumanCharacter::StaticClass(),
																	  Factory));

	CreatedPackageName = NewCharacterAssetPackageName;

	UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();

	MigrateTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("AddingToSubsystem", "Adding {0} to be edited"), FText::FromString(NewCharacterAssetName)));

	if (!Subsystem->TryAddObjectToEdit(Character))
	{
		LogError(LOCTEXT("FailedToAddToEdit", "Failed to add MetaHuman Character to subsystem for editing"));
		return;
	}

	ON_SCOPE_EXIT
	{
		Subsystem->RemoveObjectToEdit(Character);
	};

	MigrateTask.EnterProgressFrame(1.0f, LOCTEXT("ParseMigrationInfo", "Parsing Migration Info"));

	FMetaHumanMigrationInfo MigrationInfo;

	FString MigrationInfoJsonString;
	if (!FFileHelper::LoadFileToString(MigrationInfoJsonString, *MigrationInfoJsonFilePath))
	{
		LogError(FText::Format(LOCTEXT("FailedToLoadMigrationInfoJson", "Failed to load file '{0}'"), FText::FromString(MigrationInfoJsonFilePath)));
		return;
	}

	if (!FJsonObjectConverter::JsonObjectStringToUStruct(MigrationInfoJsonString, &MigrationInfo))
	{
		LogError(FText::Format(LOCTEXT("FailedToParseMigrationInfoJson", "Failed to parse '{0}'"), FText::FromString(MigrationInfoJsonFilePath)));
		return;
	}

	MigrateTask.EnterProgressFrame(1.0f, LOCTEXT("ReadingDNA", "Reading DNA file"));

	TSharedPtr<IDNAReader> DNAReader = ReadDNAFromFile(DNAFilepath);
	if (!DNAReader.IsValid())
	{
		LogError(LOCTEXT("FailToReadDNA", "Failed reading DNA"));
		return;
	}

	MigrateTask.EnterProgressFrame(1.0f, LOCTEXT("SettingBodyType", "Applying Body Type"));


	Subsystem->SetMetaHumanBodyType(Character, MigrationInfo.Body.Type,UMetaHumanCharacterEditorSubsystem::EBodyMeshUpdateMode::Full);
	Subsystem->CommitBodyState(Character, Subsystem->GetBodyState(Character));

	MigrateTask.EnterProgressFrame(1.0f, LOCTEXT("CommitFaceDNA", "Importing Face DNA"));


	// we don't need to do any alignment, or neck adaptation for this Use Case
	// Note that this leaves the Character in a non-autorigged state; the user must then autorig the Character in
	// order to use it
	FFitToTargetOptions FitToTargetOptions{ EAlignmentOptions::None, /*bAdaptNeck*/ false, /*bDisableHighFrequencyDelta*/ false };
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();

	MetaHumanCharacterSubsystem->FitToFaceDna(Character, DNAReader.ToSharedRef(), FitToTargetOptions);
	Subsystem->CommitFaceState(Character, MetaHumanCharacterSubsystem->GetFaceState(Character));

	MigrateTask.EnterProgressFrame(1.0f, LOCTEXT("SettingParameters", "Applying MetaHuman Parameters"));


	TMap<EMetaHumanMigrationDataAssetType, FMetaHumanPipelineSlotSelection> SlotSelections;

	SetSkin(Character, MigrationInfo);
	SetMakeup(Character, MigrationInfo);
	SetEyes(Character, MigrationInfo);
	SetGrooms(Character, MigrationInfo, SlotSelections);

	MigrateTask.EnterProgressFrame(1.0f, LOCTEXT("ApplyingWardrobe", "Applying Wardrobe Parameters"));
	
	UMetaHumanCollection* Collection = Character->GetMutableInternalCollection();

	UpdateWardrobe(Character, MigrationInfo, SlotSelections);
}

void FMetaHumanCharacterMigrationEditorModule::SetSkin(TNotNull<UMetaHumanCharacter*> InCharacter, const FMetaHumanMigrationInfo& InMigrationInfo)
{
	const TMap<FName, EMetaHumanCharacterFrecklesMask> FrecklesMasks =
	{
		{ TEXT("None"), EMetaHumanCharacterFrecklesMask::None },
		{ TEXT("Freckles1"), EMetaHumanCharacterFrecklesMask::Type1 },
		{ TEXT("Freckles2"), EMetaHumanCharacterFrecklesMask::Type2 },
		{ TEXT("Freckles3"), EMetaHumanCharacterFrecklesMask::Type3 },
	};

	auto GetFrecklesMask = [this, &FrecklesMasks](FName Option)
	{
		if (const EMetaHumanCharacterFrecklesMask* FoundMask = FrecklesMasks.Find(Option))
		{
			return *FoundMask;
		}
		else
		{
			LogWarning(FText::Format(LOCTEXT("InvalidFrecklesOption", "Invalid freckles option '{0}'"), FText::FromName(Option)));
		}

		return EMetaHumanCharacterFrecklesMask::None;
	};

	auto GetAccentRegionProperties = [](const FMetaHumanAccentRegionMigrationInfo& AccentRegion)
	{
		// MHC uses values in the range -1.0 to 1.0 for accent values
		// so map the values to the 0 to 1 range
		constexpr auto MapAccentValue = [](float Value) constexpr
		{
			return (Value + 1.0f) / 2.0f;
		};

		return FMetaHumanCharacterAccentRegionProperties
		{
			.Redness = MapAccentValue(AccentRegion.Redness),
			.Saturation = MapAccentValue(AccentRegion.Saturation),
			.Lightness = MapAccentValue(AccentRegion.Lightness),
		};
	};

	UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	check(Subsystem);

	// The SkinTone stored in the MigrationInfo.json is a Linear Color, so we need to convert it to sRGB as required by TS

	// Need to convert to FVector3f to use LinearToSRGB
	FVector3f SkinToneSRGB
	{
		InMigrationInfo.TextureSynthesis.SkinTone.R,
		InMigrationInfo.TextureSynthesis.SkinTone.G,
		InMigrationInfo.TextureSynthesis.SkinTone.B,
	};
	UE::Geometry::LinearColors::LinearToSRGB(SkinToneSRGB);

	// Mapping of high frequency textures from the original MHC database
	// The indices in this map are one based
	constexpr TStaticArray<int32, 56> HighFrequencyTextureMapping =
	{
		36, 17, 50, 40, 37, 27, 46, 1, 47, 35, 34, 18, 8, 13, 45, 24, 32, 33, 39,
		28, 29, 30, 38, 6, 16, 3, 19, 2, 42, 4, 10, 7, 56, 31, 44, 26, 14, 9, 41,
		23, 25, 11, 21, 5, 20, 22, 55, 54, 48, 52, 12, 51, 15, 49, 43, 53
	};

	int32 MappedSkinTextureValue = Subsystem->GetMaxHighFrequencyIndex() / 2;

	if (Subsystem->GetMaxHighFrequencyIndex() > HighFrequencyTextureMapping.Num())
	{
		// Calculate the index to access in the mapping array
		const int32 HighFrequencyTextureMapIndex = InMigrationInfo.TextureSynthesis.HighFrequency * (HighFrequencyTextureMapping.Num() - 1) + 0.5f;

		// This will be the corresponding high frequency index in the loaded texture synthesis model
		const int32 MappedHighFrequencyIndex = HighFrequencyTextureMapping[HighFrequencyTextureMapIndex] - 1;

		// store the index in the character
		MappedSkinTextureValue = MappedHighFrequencyIndex;
	}

	// Estimate the skin tone UI values from the skin tone of the incoming MetaHuman
	const FVector2f EstimatedSkinToneUI = Subsystem->EstimateSkinTone(SkinToneSRGB);

	const FMetaHumanCharacterSkinSettings SkinSettings =
	{
		.Skin =
		{
			.U = EstimatedSkinToneUI.X,
			.V = EstimatedSkinToneUI.Y,
			.FaceTextureIndex = MappedSkinTextureValue,
		},
		.Freckles =
		{
			.Density = InMigrationInfo.Face.Freckles.Density,
			.Strength = InMigrationInfo.Face.Freckles.Strength,
			.Saturation = InMigrationInfo.Face.Freckles.Saturation,
			.ToneShift = InMigrationInfo.Face.Freckles.ToneShift,
			.Mask = GetFrecklesMask(InMigrationInfo.Face.Freckles.Option)
		},
		.Accents =
		{
			.Scalp = GetAccentRegionProperties(InMigrationInfo.Face.Accents.Scalp),
			.Forehead = GetAccentRegionProperties(InMigrationInfo.Face.Accents.Forehead),
			.Nose = GetAccentRegionProperties(InMigrationInfo.Face.Accents.Nose),
			.UnderEye = GetAccentRegionProperties(InMigrationInfo.Face.Accents.UnderEye),
			.Cheeks = GetAccentRegionProperties(InMigrationInfo.Face.Accents.Cheeks),
			.Lips = GetAccentRegionProperties(InMigrationInfo.Face.Accents.Lips),
			.Chin = GetAccentRegionProperties(InMigrationInfo.Face.Accents.Chin),
			.Ears = GetAccentRegionProperties(InMigrationInfo.Face.Accents.Ears),
		}
	};

	Subsystem->CommitSkinSettings(InCharacter, SkinSettings);
}

void FMetaHumanCharacterMigrationEditorModule::SetMakeup(TNotNull<UMetaHumanCharacter*> InCharacter, const FMetaHumanMigrationInfo& InMigrationInfo)
{
	auto GetEyeMakeupMask = [this, &InMigrationInfo](FName Option)
	{
		EMetaHumanCharacterEyeMakeupType EyeMakeupType = EMetaHumanCharacterEyeMakeupType::None;

		UEnum* EyeMaskEnum = StaticEnum<EMetaHumanCharacterEyeMakeupType>();
		const int32 EyeMaskIndex = EyeMaskEnum->GetIndexByName(Option);

		if (EyeMaskIndex != INDEX_NONE)
		{
			EyeMakeupType = static_cast<EMetaHumanCharacterEyeMakeupType>(EyeMaskEnum->GetValueByIndex(EyeMaskIndex));
		}
		else
		{
			LogWarning(FText::Format(LOCTEXT("InvalidEyeOption", "Invalid eye makeup option '{0}'"), FText::FromName(Option)));
		}

		return EyeMakeupType;
	};

	auto GetBlusherMakeupMask = [this, &InMigrationInfo](FName Option)
	{
		EMetaHumanCharacterBlushMakeupType BlusherMakeupType = EMetaHumanCharacterBlushMakeupType::None;

		const TMap<FName, EMetaHumanCharacterBlushMakeupType> BlusherMasks =
		{
			{ TEXT("None"), EMetaHumanCharacterBlushMakeupType::None },
			{ TEXT("Blusher_001"), EMetaHumanCharacterBlushMakeupType::Angled },
			{ TEXT("Blusher_002"), EMetaHumanCharacterBlushMakeupType::Apple },
			{ TEXT("Blusher_003"), EMetaHumanCharacterBlushMakeupType::LowSweep },
			{ TEXT("Blusher_004"), EMetaHumanCharacterBlushMakeupType::HighCurve },
		};

		if (const EMetaHumanCharacterBlushMakeupType* FoundBlusherMask = BlusherMasks.Find(Option))
		{
			BlusherMakeupType = *FoundBlusherMask;
		}
		else
		{
			LogWarning(FText::Format(LOCTEXT("InvalidBlusherMaskOption", "Invalid blusher option '{0}'"), FText::FromName(Option)));
		}

		return BlusherMakeupType;
	};

	auto GetLipstickMakeupMask = [this, &InMigrationInfo](FName Option)
	{
		EMetaHumanCharacterLipsMakeupType LipstickMask = EMetaHumanCharacterLipsMakeupType::None;

		UEnum* LipstickMaskEnum = StaticEnum<EMetaHumanCharacterLipsMakeupType>();
		const int32 LipstickMaskIndex = LipstickMaskEnum->GetIndexByName(Option);

		if (LipstickMaskIndex != INDEX_NONE)
		{
			LipstickMask = static_cast<EMetaHumanCharacterLipsMakeupType>(LipstickMaskEnum->GetValueByIndex(LipstickMaskIndex));
		}
		else
		{
			LogWarning(FText::Format(LOCTEXT("InvalidLipstickOption", "Invalid lipstick makeup option '{0}'"), FText::FromName(Option)));
		}

		return LipstickMask;
	};

	const FMetaHumanMakeupMigrationInfo& Makeup = InMigrationInfo.Face.Makeup;

	const FMetaHumanCharacterMakeupSettings MakeupSettings =
	{
		.Foundation =
		{
			.bApplyFoundation = Makeup.Foundation.bApplyFoundation,
			.Color = Makeup.Foundation.Color,
			.Intensity = Makeup.Foundation.Intensity,
			.Roughness = Makeup.Foundation.Roughness,
			.Concealer = Makeup.Foundation.Concealer,
		},
		.Eyes =
		{
			.Type = GetEyeMakeupMask(Makeup.Eyes.Option),
			.PrimaryColor = Makeup.Eyes.PrimaryColor,
			.SecondaryColor = Makeup.Eyes.SecondaryColor,
			.Roughness = Makeup.Eyes.Roughness,
			.Opacity = 1.0f - Makeup.Eyes.Transparency,
			.Metalness = Makeup.Eyes.Metalness,
		},
		.Blush =
		{
			.Type = GetBlusherMakeupMask(Makeup.Blusher.Option),
			.Color = Makeup.Blusher.Color,
			.Intensity = Makeup.Blusher.Intensity,
			.Roughness = Makeup.Blusher.Roughness,
		},
		.Lips =
		{
			.Type = GetLipstickMakeupMask(Makeup.Lips.Option),
			.Color = Makeup.Lips.Color,
			.Roughness = Makeup.Lips.Roughness,
			.Opacity = 1.0f - Makeup.Lips.Transparency,
		}
	};

	UMetaHumanCharacterEditorSubsystem::Get()->CommitMakeupSettings(InCharacter, MakeupSettings);
}

void FMetaHumanCharacterMigrationEditorModule::SetEyes(TNotNull<class UMetaHumanCharacter*> InCharacter, const struct FMetaHumanMigrationInfo& InMigrationInfo)
{
	const FMetaHumanEyeMigrationInfo& LeftEye = InMigrationInfo.Face.LeftEye;
	const FMetaHumanEyeMigrationInfo& RightEye = InMigrationInfo.Face.RightEye;

	const FString LeftIrisPatternName = FString::Format(TEXT("Iris{0}"), { LeftEye.Iris.Option.ToString() });
	const FString RightIrisPatternName = FString::Format(TEXT("Iris{0}"), { RightEye.Iris.Option.ToString() });

	const EMetaHumanCharacterEyesIrisPattern LeftIrisPattern = static_cast<EMetaHumanCharacterEyesIrisPattern>(StaticEnum<EMetaHumanCharacterEyesIrisPattern>()->GetValueByNameString(LeftIrisPatternName));
	const EMetaHumanCharacterEyesIrisPattern RightIrisPattern = static_cast<EMetaHumanCharacterEyesIrisPattern>(StaticEnum<EMetaHumanCharacterEyesIrisPattern>()->GetValueByNameString(RightIrisPatternName));

	auto GetCorneaSize = [](float IrisSize)
	{
		return FMath::Lerp(0.145f, 0.185f, IrisSize);
	};

	// Mapping Cloud MetaHuman material properties to new VFI material parameters
	const FMetaHumanCharacterEyesSettings EyeSettings =
	{
		.EyeLeft =
		{
			.Iris =
			{
				.IrisPattern = LeftIrisPattern,
				.PrimaryColorU = static_cast<float>(LeftEye.Iris.Color1UI.X),
				.PrimaryColorV = static_cast<float>(LeftEye.Iris.Color1UI.Y),
				.SecondaryColorU = static_cast<float>(LeftEye.Iris.Color2UI.X),
				.SecondaryColorV = static_cast<float>(LeftEye.Iris.Color2UI.Y),
				.ColorBlend = LeftEye.Iris.ColorBalance,
				.ColorBlendSoftness = LeftEye.Iris.ColorBalanceSmoothness,
				.BlendMethod = LeftEye.Iris.bUseRadialBlend ? EMetaHumanCharacterEyesBlendMethod::Radial : EMetaHumanCharacterEyesBlendMethod::Structural,
				.GlobalSaturation = LeftEye.Iris.Saturation,
			},

			.Cornea =
			{
				// TODO: The mapping between MHC iris size and Cornea is not linear so this needs to be revisited
				// .Size = GetCorneaSize(LeftEye.Iris.Size),
				.LimbusColor = FLinearColor{ LeftEye.Iris.LimbusDarkAmount, LeftEye.Iris.LimbusDarkAmount, LeftEye.Iris.LimbusDarkAmount },
			},

			.Sclera =
			{
				.Rotation = LeftEye.Sclera.Rotation,
				.bUseCustomTint = true,
				.Tint = LeftEye.Sclera.Tint,
				.VascularityIntensity = LeftEye.Sclera.Vascularity,
			}
		},
		.EyeRight =
		{
			.Iris =
			{
				.IrisPattern = RightIrisPattern,
				.PrimaryColorU = static_cast<float>(RightEye.Iris.Color1UI.X),
				.PrimaryColorV = static_cast<float>(RightEye.Iris.Color1UI.Y),
				.SecondaryColorU = static_cast<float>(RightEye.Iris.Color2UI.X),
				.SecondaryColorV = static_cast<float>(RightEye.Iris.Color2UI.Y),
				.ColorBlend = RightEye.Iris.ColorBalance,
				.ColorBlendSoftness = RightEye.Iris.ColorBalanceSmoothness,
				.BlendMethod = RightEye.Iris.bUseRadialBlend ? EMetaHumanCharacterEyesBlendMethod::Radial : EMetaHumanCharacterEyesBlendMethod::Structural,
				.GlobalSaturation = RightEye.Iris.Saturation,
			},

			.Cornea =
			{
				// TODO: The mapping between MHC iris size and Cornea is not linear so this needs to be revisited
				// .Size = GetCorneaSize(RightEye.Iris.Size),
				.LimbusColor = FLinearColor{ RightEye.Iris.LimbusDarkAmount, RightEye.Iris.LimbusDarkAmount, RightEye.Iris.LimbusDarkAmount },
			},

			.Sclera =
			{
				.Rotation = RightEye.Sclera.Rotation,
				.bUseCustomTint = true,
				.Tint = RightEye.Sclera.Tint,
				.VascularityIntensity = RightEye.Sclera.Vascularity,
			}
		}
	};

	UMetaHumanCharacterEditorSubsystem::Get()->CommitEyesSettings(InCharacter, EyeSettings);
}

void FMetaHumanCharacterMigrationEditorModule::SetGrooms(
	TNotNull<UMetaHumanCharacter*> InCharacter,
	const FMetaHumanMigrationInfo& InMigrationInfo,
	TMap<EMetaHumanMigrationDataAssetType, FMetaHumanPipelineSlotSelection>& OutSlotSelections)
{
	using namespace UE::MetaHuman::Private;

	UMetaHumanCollection* Collection = InCharacter->GetMutableInternalCollection();
	const UMetaHumanCollectionEditorPipeline* Pipeline = Collection->GetEditorPipeline();

	TNotNull<const UMetaHumanMigrationDatabase*> MigrationDatabase = GetMigrationDatabase();

	auto TryAddGroom = [&](EMetaHumanMigrationDataAssetType AssetType)
	{
		FMetaHumanGroomMigrationInfo GroomInfo;

		if (!TryGetGroomMigrationInfo(InMigrationInfo, AssetType, GroomInfo))
		{
			return;
		}

		if (GroomInfo.Option.IsNone())
		{
			return;
		}

		const FName SlotName = FName(StaticEnum<EMetaHumanMigrationDataAssetType>()->GetDisplayNameTextByValue(static_cast<int32>(AssetType)).ToString());

		bool bAddOk = false;

		if (const TSoftObjectPtr<UMetaHumanWardrobeItem>* FoundWardrobeItem = MigrationDatabase->Assets[AssetType]->GroomAssetMapping.Find(GroomInfo.Option))
		{
			if (UMetaHumanWardrobeItem* WardrobeItem = FoundWardrobeItem->LoadSynchronous())
			{
				if (UGroomBindingAsset* Binding = Cast<UGroomBindingAsset>(WardrobeItem->PrincipalAsset.LoadSynchronous()))
				{
					if (Pipeline->IsPrincipalAssetClassCompatibleWithSlot(SlotName, Binding->GetClass()))
					{
						FMetaHumanPaletteItemKey NewItemKey;
						bAddOk = ensure(Collection->TryAddItemFromWardrobeItem(SlotName, WardrobeItem, NewItemKey));

						OutSlotSelections.Emplace(AssetType, FMetaHumanPipelineSlotSelection(SlotName, NewItemKey));
					}
				}
			}
		}
		
		if (!bAddOk)
		{
			LogWarning(FText::Format(LOCTEXT("FailedToLoadGroomBinding", "Requested groom is not available for slot '{0}: '{1}'"), FText::FromName(SlotName), FText::FromName(GroomInfo.Option)));
		}
	};

	const FMetaHumanGroomsMigrationInfo& GroomsMigrationInfo = InMigrationInfo.Grooms;

	TryAddGroom(EMetaHumanMigrationDataAssetType::Hair);
	TryAddGroom(EMetaHumanMigrationDataAssetType::Eyebrows);
	TryAddGroom(EMetaHumanMigrationDataAssetType::Eyelashes);
	TryAddGroom(EMetaHumanMigrationDataAssetType::Mustache);
	TryAddGroom(EMetaHumanMigrationDataAssetType::Beard);
	TryAddGroom(EMetaHumanMigrationDataAssetType::Peachfuzz);
}

void FMetaHumanCharacterMigrationEditorModule::UpdateWardrobe(
	TNotNull<UMetaHumanCharacter*> InCharacter,
	const FMetaHumanMigrationInfo& InMigrationInfo,
	const TMap<EMetaHumanMigrationDataAssetType, FMetaHumanPipelineSlotSelection>& InSlotSelections)
{
	using namespace UE::MetaHuman::Private;

	const TMap<EMetaHumanMigrationDataAssetType, EMetaHumanMigrationDataAssetType> SlotDependency =
	{
		{ EMetaHumanMigrationDataAssetType::Eyebrows, EMetaHumanMigrationDataAssetType::Hair },
		{ EMetaHumanMigrationDataAssetType::Eyelashes, EMetaHumanMigrationDataAssetType::Hair },
		{ EMetaHumanMigrationDataAssetType::Mustache, EMetaHumanMigrationDataAssetType::Hair },
		{ EMetaHumanMigrationDataAssetType::Beard, EMetaHumanMigrationDataAssetType::Hair },
		{ EMetaHumanMigrationDataAssetType::Peachfuzz, EMetaHumanMigrationDataAssetType::Hair },
	};

	UMetaHumanCollection* Collection = InCharacter->GetMutableInternalCollection();

	for (const TPair<EMetaHumanMigrationDataAssetType, FMetaHumanPipelineSlotSelection>& SelectionIt : InSlotSelections)
	{
		// Character should wear this item
		if (!Collection->GetMutableDefaultInstance()->TryAddSlotSelection(SelectionIt.Value))
		{
			// TODO: Log error
			continue;
		}

		// Groom info for this particular slot
		FMetaHumanGroomMigrationInfo CurrentGroomInfo;

		if (TryGetGroomMigrationInfo(InMigrationInfo, SelectionIt.Key, CurrentGroomInfo))
		{
			FMetaHumanGroomMigrationInfo InheritedGroomInfo = CurrentGroomInfo;

			// If there is a parent slot, check if we want to inherit these values
			if (!CurrentGroomInfo.bUseCustomProperties)
			{
				if (const EMetaHumanMigrationDataAssetType* DependencyType = SlotDependency.Find(SelectionIt.Key))
				{
					TryGetGroomMigrationInfo(InMigrationInfo, *DependencyType, InheritedGroomInfo);
				}
			}

			auto UseCustomOrWhiteColor = [](bool bUseCustom, FLinearColor CustomColor)
			{
				if (bUseCustom)
				{
					return CustomColor;
				}
				else
				{
					return FLinearColor::White;
				}
			};

			// Copy over parameters from the migration info and override default values
			FInstancedPropertyBag OverrideParams;

			// Primary colors
			{
				// Potentially inherited values
				const FMetaHumanGroomMigrationInfo& GroomInfo = InheritedGroomInfo;

				SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, Melanin), GroomInfo.MelaninAndRedness.X);
				SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, Redness), GroomInfo.MelaninAndRedness.Y);
				SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, Roughness), GroomInfo.Roughness);
				SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, Whiteness), GroomInfo.Whiteness);
				SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, Lightness), GroomInfo.Lightness);
				SetPropertyBagValueStruct<FLinearColor>(
					OverrideParams,
					GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, DyeColor),
					FConstStructView::Make(UseCustomOrWhiteColor(GroomInfo.bUseDyeColor, GroomInfo.DyeColor)));
			}

			// Secondary colors
			{
				// Secondary colors don't inherit data from its parent
				const FMetaHumanGroomMigrationInfo& GroomInfo = CurrentGroomInfo;

				// Ombre
				{
					SetPropertyBagValue<bool>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, bUseOmbre), GroomInfo.bUseOmbre);
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreU), GroomInfo.OmbreUV.X);
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreV), GroomInfo.OmbreUV.Y);
					SetPropertyBagValueStruct<FLinearColor>(
						OverrideParams,
						GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreColor),
						FConstStructView::Make(UseCustomOrWhiteColor(GroomInfo.bUseOmbreColor, GroomInfo.OmbreColor)));
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreShift), GroomInfo.OmbreShift);
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreContrast), GroomInfo.OmbreContrast);
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreIntensity), GroomInfo.OmbreIntensity);
				}

				// Regions
				{
					SetPropertyBagValue<bool>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, bUseRegions), GroomInfo.bUseRegions);
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, RegionsU), GroomInfo.RegionsUV.X);
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, RegionsV), GroomInfo.RegionsUV.Y);
					SetPropertyBagValueStruct<FLinearColor>(
						OverrideParams,
						GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, RegionsColor),
						FConstStructView::Make(UseCustomOrWhiteColor(GroomInfo.bUseRegionsColor, GroomInfo.RegionsColor)));
				}

				// Highlights
				{
					SetPropertyBagValue<bool>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, bUseHighlights), GroomInfo.bUseHighlights);
					SetPropertyBagValueStruct<FLinearColor>(
						OverrideParams,
						GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsColor),
						FConstStructView::Make(UseCustomOrWhiteColor(GroomInfo.bUseHighlightsColor, GroomInfo.HighlightsColor)));
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsU), GroomInfo.HighlightsUV.X);
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsV), GroomInfo.HighlightsUV.Y);
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsBlending), GroomInfo.HighlightsBlending);
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsIntensity), GroomInfo.HighlightsIntensity);
					SetPropertyBagValue<float>(OverrideParams, GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsVariation), static_cast<float>(GroomInfo.HighlightsVariation));
				}
			}

			Collection->GetMutableDefaultInstance()->OverrideInstanceParameters(
				SelectionIt.Value.GetSelectedItemPath(),
				OverrideParams);
		}
	}
}

void FMetaHumanCharacterMigrationEditorModule::LogWarning(const FText& InMessage)
{
	if (MessageLogPtr)
	{
		MessageLogPtr->Warning(InMessage);
		bHasWarnings = true;
	}
}

void FMetaHumanCharacterMigrationEditorModule::LogError(const FText& InMessage)
{
	if (MessageLogPtr)
	{
		MessageLogPtr->Error(InMessage);
		bHasErrors = true;
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMetaHumanCharacterMigrationEditorModule, MetaHumanCharacterMigrationEditor);
