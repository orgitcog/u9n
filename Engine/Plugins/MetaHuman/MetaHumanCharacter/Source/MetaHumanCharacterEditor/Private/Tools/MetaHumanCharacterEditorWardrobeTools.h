// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "BaseTools/SingleTargetWithSelectionTool.h"
#include "AssetRegistry/AssetData.h"

#include "MetaHumanCharacterEditorWardrobeTools.generated.h"

class UMetaHumanCharacter;
class UMetaHumanCollection;

UENUM()
enum class EMetaHumanCharacterWardrobeEditingTool : uint8
{
	Wardrobe
};

UCLASS()
class UMetaHumanCharacterEditorWardrobeToolBuilder : public UInteractiveToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual bool CanBuildTool(const FToolBuilderState& InSceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& InSceneState) const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface

	UPROPERTY()
	EMetaHumanCharacterWardrobeEditingTool ToolType = EMetaHumanCharacterWardrobeEditingTool::Wardrobe;

protected:
	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface
};

/**
 * The Detail Customization for this class contains the wardrobe editing UI
 */
UCLASS(Transient)
class UMetaHumanCharacterEditorWardrobeToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UMetaHumanCollection> Collection;

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacter> Character;
};

/**
 * The Wardrobe Tool allows the user to customize items selected in the Costume tab.
 */
UCLASS()
class UMetaHumanCharacterEditorWardrobeTool : public USingleTargetWithSelectionTool
{
	GENERATED_BODY()

public:
	/** Get the Wardrobe Tool properties. */
	UMetaHumanCharacterEditorWardrobeToolProperties* GetWardrobeToolProperties() const { return PropertyObject; }

	//~Begin UInteractiveTool interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;
	
	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }
	virtual bool CanAccept() const override { return false; }
	//~End UInteractiveTool interface

protected:
	void OnWardrobePathsChanged();

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorWardrobeToolProperties> PropertyObject;

	FDelegateHandle WardrobePathChangedUserSettings;
	FDelegateHandle WardrobePathChangedCharacter;
};
