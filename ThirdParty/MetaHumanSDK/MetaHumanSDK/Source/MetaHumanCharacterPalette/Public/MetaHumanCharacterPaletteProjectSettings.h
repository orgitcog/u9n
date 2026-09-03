// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"

#include "MetaHumanCharacterPaletteProjectSettings.generated.h"

enum class EMetaHumanQualityLevel : uint8;

UCLASS(Config = MetaHumanCharacter, DefaultConfig)
class METAHUMANCHARACTERPALETTE_API UMetaHumanCharacterPaletteProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMetaHumanCharacterPaletteProjectSettings();

	// Begin UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("MetaHumanCharacterPalette"); }
#if WITH_EDITOR
	virtual FText GetSectionText() const override { return NSLOCTEXT("MetaHumanCharacterPaletteProjectSettings", "SettingsName", "MetaHuman Character Build"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("MetaHumanCharacterProjectSettings", "SettingsDescription", "Configuration settings for MetaHuman Character build features"); }
#endif // WITH_EDITOR
	// End UDeveloperSettings interface

	/** The default MetaHuman Collection Pipeline that will be used by new MetaHuman Characters and Collections */
	UPROPERTY(Config, EditAnywhere, Category = "Build")
	TSoftClassPtr<class UMetaHumanCollectionPipeline> DefaultCharacterPipelineClass;

	/** The default legacy MetaHuman Collection Pipelines that is used to build legacy Actor-based blueprints. */
	UPROPERTY(Config, EditAnywhere, Category = "Build", EditFixedSize, NoClear)
	TMap<EMetaHumanQualityLevel, TSoftClassPtr<class UMetaHumanCollectionPipeline>> DefaultCharacterLegacyPipelines;

	UPROPERTY(Config, EditAnywhere, Category = "Build", EditFixedSize, NoClear)
	TMap<EMetaHumanQualityLevel, TSoftClassPtr<class UMetaHumanCollectionPipeline>> DefaultCharacterUEFNPipelines;
};
