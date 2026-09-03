// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterEditorSubsystem.h"
#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
class SCanvas;

DECLARE_DELEGATE_OneParam(FOnRegionClicked, EMetaHumanCharacterAccentRegion)

/** Struct used for holding informations of an Accent Region. */
struct FMetaHumanCharacterEditorAccentRegionInfo
{
	EMetaHumanCharacterAccentRegion Type;
	FName Name;
	FText Label;
	FVector2D Position;
	FVector2D Size;
};

/**
 * Widget used to display the Skin Accent Regions.
 *
 * This widget is created from check boxes since they can display brushes for checked and hovered states
 * The brushes for each check box are defined in the style class FMetaHumanCharacterEditorStyle so each
 * region check box just need to reference the style and the correct images are going to be used.
 * This widget is completely passive, meaning it doesn't hold any selection state. The selected region
 * is defined by an attribute that each check box can use to compare if it should be in the checked state or not
 *
 * The widget has two main parts, the background Head image and the regions the user can click. The regions
 * are built using an SCanvas, which is a widget that allows anchoring its child widgets as well set custom
 * sizes for each
 */
class SMetaHumanCharacterEditorAccentRegionsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorAccentRegionsPanel) 
		: _SelectedRegion(EMetaHumanCharacterAccentRegion::Scalp)
		{}
		SLATE_ATTRIBUTE(EMetaHumanCharacterAccentRegion, SelectedRegion)
		SLATE_EVENT(FOnRegionClicked, OnRegionClicked)
	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs);

	/** Gets the currently selected Accent Region. */
	EMetaHumanCharacterAccentRegion GetSelectedRegion() const;

private:
	/** Makes the Accent Regions main canvas widget. */
	void MakeAccentRegionsCanvas(const TArray<FMetaHumanCharacterEditorAccentRegionInfo> AccentRegionsInfos);

	/** Creates the array of Accent Regions info used to correctly create the widget. */
	TArray<FMetaHumanCharacterEditorAccentRegionInfo> CreateAccentRegionsInfoArray() const;

	/** Gets the check box state which tells if a specific Accent Region is selected or not. */
	ECheckBoxState IsRegionSelected(EMetaHumanCharacterAccentRegion InRegion) const;

	/** Called when the check box state of a specific Accent Region has been changed. */
	void OnRegionCheckedStateChanged(ECheckBoxState InState, EMetaHumanCharacterAccentRegion InRegion);

	/** Delegate executed when the user clicks on a region. */
	FOnRegionClicked RegionClickedDelegate;

	/** Reference to the Accent Regions canvas. */
	TSharedPtr<SCanvas> AccentRegionsCanvas;

	/** Attribute used to query the selected region. */
	TAttribute<EMetaHumanCharacterAccentRegion> SelectedRegion;
};
