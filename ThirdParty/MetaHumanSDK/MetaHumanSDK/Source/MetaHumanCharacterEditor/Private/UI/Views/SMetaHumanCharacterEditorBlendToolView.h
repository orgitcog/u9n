// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SMetaHumanCharacterEditorToolView.h"

struct FMetaHumanCharacterAssetViewItem;
class SMetaHumanCharacterEditorBlendToolPanel;
class UMetaHumanCharacter;
class UMetaHumanCharacterEditorMeshBlendTool;
class UMetaHumanCharacterEditorBodyBlendToolProperties;

/** View for displaying the Blend Tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorBlendToolView
	: public SMetaHumanCharacterEditorToolView
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorBlendToolView)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorMeshBlendTool* InTool);

	/** Called when an item is dropped in the blend tool panel. */
	void OnBlendToolItemDropped(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, int32 InItemIndex);

	/** Called when an item is deleted in the blend tool panel. */
	void OnBlendToolItemDeleted(int32 InItemIndex);

	/** Called when an item is double clicked in the presets of the blend tool panel. */
	void OnBlendToolItemActivated(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item);

protected:
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual UInteractiveToolPropertySet* GetToolProperties() const override;
	//~ End of SMetaHumanCharacterEditorToolView interface

	/** Creates the section widget for showing the Presets properties. */
	virtual TSharedRef<SWidget> CreateBlendToolViewBlendPanelSection() = 0;

	/** Called to override the item thumbnail brush. */
	virtual void OnOverrideItemThumbnailBrush(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const = 0;

	/** Reference to the Blend Tool panel, used for handling preset blending. */
	TSharedPtr<SMetaHumanCharacterEditorBlendToolPanel> BlendToolPanel;
};

class SMetaHumanCharacterEditorHeadBlendToolView
	: public SMetaHumanCharacterEditorBlendToolView
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorHeadBlendToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorMeshBlendTool* InTool);

	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual void MakeToolView() override;
	//~ End of SMetaHumanCharacterEditorToolView interface

protected:
	/** Creates the section widget for showing the head blend properties. */
	virtual TSharedRef<SWidget> CreateBlendToolViewBlendPanelSection() override;

	/** Called to override the item thumbnail brush. */
	virtual void OnOverrideItemThumbnailBrush(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const override;

	/** Name identifier for the slot where virtual assets from the head blend tool are stored. */
	static FName HeadBlendAssetsSlotName;

	/** Creates the section widget for showing the manipulator properties. */
	TSharedRef<SWidget> CreateManipulatorsViewSection();

	/** Creates the section widget for showing the head parameter properties. */
	TSharedRef<SWidget> CreateHeadParametersViewSection();

	/** Reset the head model */
	FReply OnResetButtonClicked() const;

	/** Revert the neck region of the head based on the body */
	FReply OnResetNeckButtonClicked() const;
};

class SMetaHumanCharacterEditorBodyBlendToolView
	: public SMetaHumanCharacterEditorBlendToolView
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorBodyBlendToolView)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorMeshBlendTool* InTool);
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual void MakeToolView() override;
	//~ End of SMetaHumanCharacterEditorToolView interface

protected:
	/** Creates the section widget for showing the head blend properties. */
	virtual TSharedRef<SWidget> CreateBlendToolViewBlendPanelSection() override;

	/** Creates the section widget for showing the manipulator properties. */
	TSharedRef<SWidget> CreateManipulatorsViewSection();

	/** Creates the section widget for showing body head parameter properties. */
	TSharedRef<SWidget> CreateBodyParametersViewSection();

	/** Called to override the item thumbnail brush. */
	virtual void OnOverrideItemThumbnailBrush(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const override;

	/** Called to override the item thumbnail brush. */
	virtual bool OnFilterAddAssetDataToAssetView(const FAssetData& AssetData) const;

	/** Returns the body blend tool properties */
	UMetaHumanCharacterEditorBodyBlendToolProperties* GetBodyBlendToolProperties() const;

private:
	/** Gets the visibility for the fixed body warning. */
	EVisibility GetBodyBlendSubToolVisibility() const;

	/** Gets the visibility for the fixed body warning. */
	EVisibility GetFixedBodyWarningVisibility() const;

	/* Perform parameteric fit for fixed body types */
	FReply OnPerformParametricFitButtonClicked() const;

	/** Reset the body model */
	FReply OnResetButtonClicked() const;

	/** Name identifier for the slot where virtual assets from the body blend tool are stored. */
	static FName BodyBlendAssetsSlotName;
};