// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "SingleSelectionTool.h"
#include "MetaHumanCharacter.h"
#include "Engine/DataAsset.h"

#include "MetaHumanCharacterEditorEyesTool.generated.h"

/**
 * Data that represents an eye preset that can be displayed in the eye tool
 */
USTRUCT()
struct FMetaHumanCharacterEyePreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Preset")
	FName PresetName;

	UPROPERTY(EditAnywhere, Category = "Eye Settings")
	FMetaHumanCharacterEyesSettings EyesSettings;

	UPROPERTY(EditAnywhere, Category = "Thumbnail")
	TSoftObjectPtr<UTexture> Thumbnail;
};

/**
 * Data Asset definition for eye presets
 */
UCLASS()
class UMetaHumanCharacterEyePresets : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Returns the default data asset used for eye presets */
	static TNotNull<UMetaHumanCharacterEyePresets*> Get();

public:

	// The list of eye presets the user can select
	UPROPERTY(EditAnywhere, Category = "Presets")
	TArray<FMetaHumanCharacterEyePreset> Presets;
};

UENUM()
enum class EMetaHumanCharacterEyeEditSelection : uint8
{
	Both,
	Left,
	Right,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterEyeEditSelection, EMetaHumanCharacterEyeEditSelection::Count);

UCLASS()
class UMetaHumanCharacterEditorEyesToolBuilder : public UInteractiveToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual bool CanBuildTool(const FToolBuilderState& InSceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& InSceneState) const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface

protected:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface
};

UCLASS()
class UMetaHumanCharacterEditorEyesToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	void CopyFrom(const FMetaHumanCharacterEyesSettings& InEyesSettings);
	void CopyTo(FMetaHumanCharacterEyesSettings& OutEyesSettings) const;

public:

	UPROPERTY(EditAnywhere, Category = "Selection")
	EMetaHumanCharacterEyeEditSelection EyeSelection = EMetaHumanCharacterEyeEditSelection::Both;

	UPROPERTY(EditAnywhere, Category = "Eye", meta = (ShowOnlyInnerProperties), Transient)
	FMetaHumanCharacterEyeProperties Eye;
};

UCLASS()
class UMetaHumanCharacterEditorEyesTool : public USingleSelectionTool
{
	GENERATED_BODY()

public:
	//~Begin USingleTargetWithSelectionTool interface
	virtual void Setup();
	virtual void Shutdown(EToolShutdownType InShutdownType);

	virtual bool HasCancel() const { return true; }
	virtual bool HasAccept() const { return true; }
	virtual bool CanAccept() const { return true; }

	virtual void OnPropertyModified(UObject* InPropertySet, FProperty* InProperty) override;
	//~End USingleTargetWithSelectionTool interface

	/** Get the Eyes Tool properties. */
	UMetaHumanCharacterEditorEyesToolProperties* GetEyesToolProperties() const;

	void SetEyeSelection(EMetaHumanCharacterEyeEditSelection InSelection);

	void SetEyesFromPreset(const FMetaHumanCharacterEyesSettings& InPreset);

private:

	friend class FEyesToolCommandChange;

	/** Properties of the Eyes Tool. These are displayed in the details panel when the tool is activated. */
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorEyesToolProperties> EyesProperties;
};