// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SingleSelectionTool.h"
#include "MetaHumanCharacterEditorSubTools.h"

#include "MetaHumanCharacterEditorPresetsTool.generated.h"

UCLASS()
class UMetaHumanCharacterEditorPresetsToolBuilder : public UMetaHumanCharacterEditorToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& InSceneState) const override;
	virtual bool CanBuildTool(const FToolBuilderState& InSceneState) const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface

protected:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface
};

UENUM()
enum class EAssetThumbnailAcquisitionType : uint8
{
	Camera		UMETA(DisplayName = "Use Face Camera"),
	Custom		UMETA(DisplayName = "Use Custom Image"),

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EAssetThumbnailAcquisitionType, EAssetThumbnailAcquisitionType::Count);

USTRUCT()
struct FMetaHumanCharacterPresetsManagementProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tags")
	FString Factory;

	UPROPERTY(EditAnywhere, Category = "Tags")
	FString User;

	UPROPERTY(EditAnywhere, Category = "Thumbnail")
	bool bUseAssetThumbnail = false;

	UPROPERTY(EditAnywhere, Category = "Thumbnail")
	EAssetThumbnailAcquisitionType AssetThumbnail = EAssetThumbnailAcquisitionType::Custom;

	UPROPERTY(EditAnywhere, Category = "Thumbnail")
	FDirectoryPath ImagePath;
};

USTRUCT()
struct FMetaHumanCharacterPresetsLibraryProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Library Management")
	FDirectoryPath Path;

	UPROPERTY(EditAnywhere, Category = "Library Management", meta = (ContentDir))
	FDirectoryPath ProjectPath;
};

UCLASS()
class UMetaHumanCharacterEditorPresetsToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	//~Begin UObject interface
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	//~End UObject interface

public:
	UPROPERTY(EditAnywhere, Category = "Presets Management")
	FMetaHumanCharacterPresetsManagementProperties PresetsManagement;

	UPROPERTY(EditAnywhere, Category = "Library Management")
	FMetaHumanCharacterPresetsLibraryProperties LibraryManagement;
};

UCLASS()
class UMetaHumanCharacterEditorPresetsTool : public USingleSelectionTool
{
	GENERATED_BODY()

public:
	/** Get the Presets Tool properties. */
	UMetaHumanCharacterEditorPresetsToolProperties* GetPresetsToolProperties() const { return PresetsProperties; }

	//~Begin USingleTargetWithSelectionTool interface
	virtual void Setup();
	virtual void Shutdown(EToolShutdownType InShutdownType);

	virtual bool HasCancel() const { return true; }
	virtual bool HasAccept() const { return true; }
	virtual bool CanAccept() const { return true; }

	virtual void OnPropertyModified(UObject* InPropertySet, FProperty* InProperty) override;
	//~End USingleTargetWithSelectionTool interface

	void ApplyPresetCharacter(TNotNull<class UMetaHumanCharacter*> InPresetCharacter);

private:

	friend class FPresetsToolCommandChange;

	/** Properties of the Presets Tool. These are displayed in the details panel when the tool is activated. */
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorPresetsToolProperties> PresetsProperties;
};
