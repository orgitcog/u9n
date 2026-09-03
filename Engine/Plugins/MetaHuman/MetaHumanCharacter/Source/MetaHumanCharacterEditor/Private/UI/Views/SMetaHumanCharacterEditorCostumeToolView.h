// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "EditorUndoClient.h"
#include "SMetaHumanCharacterEditorToolView.h"

class SVerticalBox;
class UMetaHumanCharacterEditorCostumeTool;

/** View for displaying the Costume Tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorCostumeToolView
	: public SMetaHumanCharacterEditorToolView
	, public FSelfRegisteringEditorUndoClient
	, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorCostumeToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorCostumeTool* InTool);

protected:
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual UInteractiveToolPropertySet* GetToolProperties() const override;
	virtual void MakeToolView() override;
	//~ End of SMetaHumanCharacterEditorToolView interface

	//~ Begin FNotifyHook interface
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	//~ End of FNotifyHook interface

	//~ Begin FSelfRegisteringEditorUndoClient interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FSelfRegisteringEditorUndoClient interface

private:
	/** Creates the section widget for showing the Warning panel. */
	TSharedRef<SWidget> CreateCostumeToolViewWarningSection();

	/** Creates the section widget for showing the Grooms properties. */
	TSharedRef<SWidget> CreateCostumeToolViewGroomsSection();

	/** Creates the section widget for showing the Outfit Clothing properties. */
	TSharedRef<SWidget> CreateCostumeToolViewOutfitClothingSection();

	/** Creates the section widget for showing the Skeletal Mesh properties. */
	TSharedRef<SWidget> CreateCostumeToolViewSkeletalMeshSection();

	/** Makes the costume item boxes content, based on the current wardrobe selection. */
	void MakeCostumeItemsBoxes();

	/** Gets the visibility of the warning panel */
	EVisibility GetWarningVisibility() const;

	/** Gets the visibility of the grooms container box. */
	EVisibility GetGroomsBoxVisibility() const;

	/** Gets the visibility of the outfit clothing container box. */
	EVisibility GetOutfitClothingBoxVisibility() const;

	/** Gets the visibility of the skeletal mesh container box. */
	EVisibility GetSkeletalMeshBoxVisibility() const;

	/** True if the given name is one of the given enum's values. */
	bool IsNameEnumValue(UEnum* EnumPtr, const FName& NameToCheck) const;

	/** Refreshes the panel asset views widgets. */
	void Refresh();

	/** Reference to the container box for grooms. */
	TSharedPtr<SVerticalBox> GroomsBox;

	/** Reference to the container box for outfit clothing. */
	TSharedPtr<SVerticalBox> OutfitClothingBox;

	/** Reference to the container box for skeletal meshes. */
	TSharedPtr<SVerticalBox> SkeletalMeshesBox;
};
