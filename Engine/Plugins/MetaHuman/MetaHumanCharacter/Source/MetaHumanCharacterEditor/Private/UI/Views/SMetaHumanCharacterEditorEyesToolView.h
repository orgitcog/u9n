// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SMetaHumanCharacterEditorToolView.h"

using FEyePresetItemPtr = TSharedPtr<struct FEyePresetItem>;

struct FSlateBrush;
class UMetaHumanCharacterEditorEyesTool;

/** View for displaying the Eyes Tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorEyesToolView
	: public SMetaHumanCharacterEditorToolView
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorEyesToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorEyesTool* InTool);

protected:
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual UInteractiveToolPropertySet* GetToolProperties() const override;
	virtual void MakeToolView() override;
	//~ End of SMetaHumanCharacterEditorToolView interface

private:
	/** Creates the section widget for showing the Presets properties. */
	TSharedRef<SWidget> CreateEyesToolViewPresetsSection();

	/** Creates the section widget for showing the eye selction */
	TSharedRef<SWidget> CreateEyeSelectionSection();

	/** Creates the section widget for showing the Iris properties. */
	TSharedRef<SWidget> CreateEyesToolViewIrisSection();

	/** Creates the section widget for showing the Pupil properties */
	TSharedRef<SWidget> CreateEyeToolViewPupilSection();

	/** Creates the section widget for showing the Cornea properties */
	TSharedRef<SWidget> CreateEyeCorneaViewSection();

	/** Creates the section widget for showing the Sclera properties. */
	TSharedRef<SWidget> CreateEyesToolViewScleraSection();

	/** Gets the Iris section brush according to the given item value. */
	const FSlateBrush* GetIrisSectionBrush(uint8 InItem);

private:

	// List of preset items to display in the Preset Tile View
	TArray<FEyePresetItemPtr> PresetItems;

};
