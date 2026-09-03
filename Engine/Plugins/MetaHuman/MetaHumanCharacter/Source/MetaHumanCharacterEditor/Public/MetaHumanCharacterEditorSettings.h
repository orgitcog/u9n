// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/Scene.h"
#include "Delegates/Delegate.h"
#include "MetaHumanCharacter.h"

#include "MetaHumanCharacterEditorSettings.generated.h"

UENUM()
enum class EMetaHumanCharacterMigrationAction : uint8
{
	// When adding a MetaHuman, prompt for the action to take
	Prompt,

	// Import the legacy MetaHuman to the project
	Import,

	// Migrate the MetaHuman to its new representation
	Migrate,

	// Performs both an import and migrate operations
	ImportAndMigrate,
};

/**
 * Settings for the Modeling Tools Editor Mode plug-in.
 */
UCLASS(config = EditorPerProjectUserSettings)
class METAHUMANCHARACTEREDITOR_API UMetaHumanCharacterEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Constructor */
	UMetaHumanCharacterEditorSettings();

	//~Begin UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& InTransactionEvent) override;
	//~End UObject interface

	//~Begin UDeveloperSettings interface
	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("MetaHumanCharacter"); }

	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
	//~End UDeveloperSettings interface

	/** Gets a reference to the OnPresetsDirectoriesChanged delegate */
	FSimpleDelegate& GetOnPresetsDirectoriesChanged() { return OnPresetsDirectoriesChanged; }

private:
	/** The delegate executed when the presets directory paths have been changed */
	FSimpleDelegate OnPresetsDirectoriesChanged;

public:

	UPROPERTY(Config, EditAnywhere, Category = "Face Editing|Texture Synthesis", DisplayName = "Texture Synthesis Model Directory")
	FDirectoryPath TextureSynthesisModelDir;

	UPROPERTY(Config, EditAnywhere, Category = "Face Editing|Texture Synthesis")
	int32 TextureSynthesisThreadCount = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Face Editing|Manipulators")
	TSoftObjectPtr<class UStaticMesh> SculptManipulatorMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Face Editing|Manipulators")
	TSoftObjectPtr<class UStaticMesh> MoveManipulatorMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Fixed Body Types")
	bool bShowCompatibilityModeBodies = false;

	UPROPERTY(Config, EditAnywhere, Category = "Pipeline")
	bool bEnableExperimentalWorkflows = false;

	UPROPERTY(Config, EditAnywhere, Category = "Animation")
	TArray<FSoftObjectPath> TemplateAnimationDataTableAssets;

	// Where MetaHuman Character presets are going to be searched
	UPROPERTY(Config, EditAnywhere, Category = "Presets", DisplayName = "Presets Directories", meta = (ContentDir))
	TArray<FDirectoryPath> PresetsDirectories;

	// What happens when adding a MetaHuman from Bridge
	UPROPERTY(Config, EditAnywhere, Category = "Migration")
	EMetaHumanCharacterMigrationAction MigrationAction = EMetaHumanCharacterMigrationAction::Prompt;

	// Where new MetaHuman Character assets are going to be placed
	UPROPERTY(Config, EditAnywhere, Category = "Migration", meta = (ContentDir))
	FDirectoryPath MigratedPackagePath;

	// Prefix to be added to the name of the migrated MetaHuman Character asset
	UPROPERTY(Config, EditAnywhere, Category = "Migration")
	FString MigratedNamePrefix;

	// Suffix to be added to the name of the migrated MetaHuman Character asset
	UPROPERTY(Config, EditAnywhere, Category = "Migration")
	FString MigratedNameSuffix;

	// Boost factor to apply when streaming textures in the MetaHumanCharacter asset editor. A higher boost value will stream higher resolution textures in the viewport
	UPROPERTY(Config, EditAnywhere, Category = "Viewport", meta = (ClampMin = "1", ClampMax = "5"))
	int32 TextureStreamingBoost = 5;

	UPROPERTY(Config, EditAnywhere, Category = "Camera Options", Meta = (ClampMin = "0", ClampMax = "10"))
	int32 CameraSpeed = 2;

	UPROPERTY(Config, EditAnywhere, Category = "Camera Options", Meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MouseSensitivityModifier = 0.4;

	/** User defined wardrobe paths */
	UPROPERTY(config, EditAnywhere, Category = "Wardrobe", meta=(DisplayName="New Wardrobe Default Asset Paths"))
	TArray<FMetaHumanCharacterAssetsSection> WardrobePaths;

	UPROPERTY(config, EditAnywhere, Category = "RenderingQuality", EditFixedSize, NoClear)
	TMap<EMetaHumanCharacterRenderingQuality, FPostProcessSettings> DefaultRenderingQualities;

	/** Triggers when we change the wardrobe paths */
	DECLARE_MULTICAST_DELEGATE(FOnWardrobePathsChanged);
	FOnWardrobePathsChanged OnWardrobePathsChanged;

	/** The delegate executed when the experimental assembly options enable state has changed */
	FSimpleDelegate OnExperimentalAssemblyOptionsStateChanged;
};
