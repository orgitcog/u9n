// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "UObject/NameTypes.h"
#include "Internationalization/Text.h"

#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorWardrobeSettings.generated.h"

/**
 * Predefined wardrobe settings.
 */
UCLASS(Config = Editor, DefaultConfig)
class METAHUMANCHARACTEREDITOR_API UMetaHumanCharacterEditorWardrobeSettings : public UObject
{
	GENERATED_BODY()

public:
	/** List of predefined sections to display assets from */
	UPROPERTY(Config, EditAnywhere, Category = "Wardrobe")
	TArray<FMetaHumanCharacterAssetsSection> WardrobeSections;

	/** Mapping between slot name (e.g. Hair, Beard) and the display name for the category */
	UPROPERTY(Config, EditAnywhere, Category = "Wardrobe")
	TMap<FName, FText> SlotNameToCategoryNameMap;

	/** Mapping between Eyelashes type and corresponding groom wardrobe asset. */
	UPROPERTY(Config, EditAnywhere, Category = "Wardrobe")
	TMap<EMetaHumanCharacterEyelashesType, FSoftObjectPath> EyelashesTypeToAssetPath;

	/** List of predefined preset directories to show by default */
	UPROPERTY(Config, EditAnywhere, Category = "Preset")
	TArray<FDirectoryPath> PresetDirectories;

	FText SlotNameToCategoryName(const FName& InSlotName, const FText& InTextFallback = FText::GetEmpty()) const;
};
