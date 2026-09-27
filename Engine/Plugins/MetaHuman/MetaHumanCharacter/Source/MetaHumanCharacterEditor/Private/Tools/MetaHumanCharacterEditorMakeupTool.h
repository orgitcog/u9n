// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "SingleSelectionTool.h"
#include "MetaHumanCharacter.h"

#include "MetaHumanCharacterEditorMakeupTool.generated.h"

UCLASS()
class UMetaHumanCharacterEditorMakeupToolBuilder : public UInteractiveToolWithToolTargetsBuilder
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
class UMetaHumanCharacterEditorMakeupToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	void CopyFrom(const FMetaHumanCharacterMakeupSettings& InMakeupSettings);
	void CopyTo(FMetaHumanCharacterMakeupSettings& OutMakeupSettings);

public:

	UPROPERTY(EditAnywhere, Category = "Foundation", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterFoundationMakeupProperties Foundation;

	UPROPERTY(EditAnywhere, Category = "Eyes", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterEyeMakeupProperties Eyes;

	UPROPERTY(EditAnywhere, Category = "Blush", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterBlushMakeupProperties Blush;

	UPROPERTY(EditAnywhere, Category = "Lips", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterLipsMakeupProperties Lips;

};

UCLASS()
class UMetaHumanCharacterEditorMakeupTool : public USingleSelectionTool
{
	GENERATED_BODY()

public:
	/** Get the Makeup Tool properties. */
	UMetaHumanCharacterEditorMakeupToolProperties* GetMakeupToolProperties() const { return MakeupProperties; }

	//~Begin USingleTargetWithSelectionTool interface
	virtual void Setup();
	virtual void Shutdown(EToolShutdownType InShutdownType);

	virtual bool HasCancel() const { return true; }
	virtual bool HasAccept() const { return true; }
	virtual bool CanAccept() const { return true; }

	virtual void OnPropertyModified(UObject* InPropertySet, FProperty* InProperty) override;
	//~End USingleTargetWithSelectionTool interface

private:

	void UpdateMakeupSettings();

private:

	friend class FMakeupToolCommandChange;

	/** Properties of the Makeup Tool. These are displayed in the details panel when the tool is activated. */
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorMakeupToolProperties> MakeupProperties;

	/** Keep track of previously set makeup settings */
	FMetaHumanCharacterMakeupSettings PreviousMakeupSettings;

	/** Keep track of whether the tool applied any changes */
	bool bActorWasModified = false;
};