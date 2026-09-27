// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "SMetaHumanCharacterEditorToolView.h"

struct FSlateBrush;
class SMetaHumanCharacterEditorAccentRegionsPanel;
class UMetaHumanCharacterEditorSkinTool;

/** View for displaying the Skin Tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorSkinToolView
	: public SMetaHumanCharacterEditorToolView
	, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorSkinToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorSkinTool* InTool);

protected:
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual UInteractiveToolPropertySet* GetToolProperties() const override;
	virtual void MakeToolView() override;
	//~ End of SMetaHumanCharacterEditorToolView interface

	//~ Begin FNotifyHook interface
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	//~ End of FNotifyHook interface

private:
	/** Creates the section widget for showing the Skin properties. */
	TSharedRef<SWidget> CreateSkinToolViewSkinSection();

	/** Creates the section widget for showing the Freckles properties. */
	TSharedRef<SWidget> CreateSkinToolViewFrecklesSection();

	/** Creates the section widget for showing the Accents properties. */
	TSharedRef<SWidget> CreateSkinToolViewAccentsSection();

	/** Creates the section widget for showing the Texture Overrides properties. */
	TSharedRef<SWidget> CreateSkinToolViewTextureOverridesSection();

	/** Creates a spin box widget for displaying accent region related Numeric Properties, if valid. */
	TSharedRef<SWidget> CreateAccentRegionPropertySpinBoxWidget(const FString Label, FProperty* Property, const int32 FractionalDigits = 2);

	/** Gets the specific accent region Property Container pointer from the current panel selection. */
	void* GetAccentRegionPropertyContainerFromSelection() const;

	/** Gets the value of the given accent region related Numeric Property, if valid. */
	TOptional<float> GetAccentRegionFloatPropertyValue(FProperty* Property) const;

	/** Called when the value of an accent region related Numeric Property is changed, if valid. */
	void OnAccentRegionFloatPropertyValueChanged(const float Value, bool bIsDragging, FProperty* Property);

	/** Called when the Skin UV values have changed. */
	void OnSkinUVChanged(const FVector2f& UV, bool bIsDragging);

	/** Gets the Freckles section brush according to the given item value. */
	const FSlateBrush* GetFreckesSectionBrush(uint8 InItem);

	/** True if editing is enabled in this tool view. */
	bool IsEditEnabled() const;

	/** True if skin editing is enabled in this tool view. */
	bool IsSkinEditEnabled() const;

	/** Returns whether the warning for skin editing should be visible. */
	EVisibility GetSkinEditWarningVisibility() const;

	/** Returns the visibility of a property. */
	virtual EVisibility GetPropertyVisibility(FProperty* Property, void* PropertyContainerPtr) const override;

	/** Returns whether the property is enabled */
	virtual bool IsPropertyEnabled(FProperty* Property, void* PropertyContainerPtr) const override;

	/** Reference to the Accent Regions panel. */
	TSharedPtr<SMetaHumanCharacterEditorAccentRegionsPanel> AccentRegionsPanel;

	/** Attribute names. */
	TArray<TArray<TSharedPtr<FString>>> AttributeValueNames;
};
