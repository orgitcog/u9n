// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SMetaHumanCharacterEditorToolView.h"

struct FSlateBrush;
class UMetaHumanCharacterEditorMakeupTool;

/** View for displaying the Makeup Tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorMakeupToolView
	: public SMetaHumanCharacterEditorToolView
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorMakeupToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorMakeupTool* InTool);

protected:
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual UInteractiveToolPropertySet* GetToolProperties() const override;
	virtual void MakeToolView() override;
	//~ End of SMetaHumanCharacterEditorToolView interface

private:
	/** Creates the section widget for showing the Foundation properties. */
	TSharedRef<SWidget> CreateMakeupToolViewFoundationSection();

	/** Creates the section widget for showing the Eyes properties. */
	TSharedRef<SWidget> CreateMakeupToolViewEyesSection();

	/** Creates the section widget for showing the Blush properties. */
	TSharedRef<SWidget> CreateMakeupToolViewBlushSection();
	
	/** Creates the section widget for showing the Lips properties. */
	TSharedRef<SWidget> CreateMakeupToolViewLipsSection();

	/** Gets the Eyes section brush according to the given item value. */
	const FSlateBrush* GetEyesSectionBrush(uint8 InItem);

	/** Gets the Blush section brush according to the given item value. */
	const FSlateBrush* GetBlushSectionBrush(uint8 InItem);

	/** Gets the Lips section brush according to the given item value. */
	const FSlateBrush* GetLipsSectionBrush(uint8 InItem);
};
