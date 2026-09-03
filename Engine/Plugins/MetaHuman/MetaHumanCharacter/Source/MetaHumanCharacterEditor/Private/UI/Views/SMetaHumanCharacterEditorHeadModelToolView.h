// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SMetaHumanCharacterEditorToolView.h"

enum class EMetaHumanCharacterTeethPropertyType : uint8;
struct FSlateBrush;
template<typename TEnum> class SMetaHumanCharacterEditorComboBox;
class SVerticalBox;
class UMetaHumanCharacterEditorHeadModelTool;

/** View for displaying the HeadModel Tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorHeadModelToolView
	: public SMetaHumanCharacterEditorToolView
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorHeadModelToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorHeadModelTool* InTool);

protected:
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual UInteractiveToolPropertySet* GetToolProperties() const override;
	virtual void MakeToolView() override;
	//~ End of SMetaHumanCharacterEditorToolView interface

private:
	/** Gets the Head Model Eyelashes subtool properties. */
	UInteractiveToolPropertySet* GetEyelashesProperties() const;

	/** Gets the Head Model Teeth subtool properties. */
	UInteractiveToolPropertySet* GetTeethProperties() const ;

	/** Makes the Eyelashes subtool view. */
	void MakeEyelashesSubToolView();

	/** Makes the Teeth subtool view. */
	void MakeTeethSubToolView();

	/** gets called when the subtool changes */
	void OnPropertySetsModified();

	/** Creates the section widget for showing the Eyelashes style properties. */
	TSharedRef<SWidget> CreateEyelashesSubToolViewStyleSection();

	/** Creates the section widget for showing the Eyelashes material properties. */
	TSharedRef<SWidget> CreateEyelashesSubToolViewMaterialSection();

	/** Creates the section widget for showing the Teeth parameters properties. */
	TSharedRef<SWidget> CreateTeethSubToolViewParametersSection();

	/** Called when a property is being edited on a teeth slider. */
	void OnTeethSliderPropertyEdited(FProperty* Property);

	/** Called when the teeth editable property has changed. */
	void OnTeethEditablePropertyValueChanged(uint8 Value, FProperty* Property, void* PropertyContainerPtr);

	/** Called when the value of a property has been changed on a teeth slider. */
	void OnTeethSliderValueChanged(const float Value, bool bIsInteractive, FProperty* Property, void* PropertyContainerPtr);

	/** Gets the Eyelashes section brush according to the given item value. */
	const FSlateBrush* GetEyelashesSectionBrush(uint8 InItem);

	/** Gets the Teeth section brush according to the given item value. */
	const FSlateBrush* GetTeethSectionBrush(uint8 InItem);

	/** Gets the visibility for the Eyelashes subtool view. */
	EVisibility GetEyelashesSubToolViewVisibility() const;

	/** Gets the visibility for the Teeth subtool view. */
	EVisibility GetTeethSubToolViewVisibility() const;

	/** Reference to the box container of the current teeth editable property. */
	TSharedPtr<SVerticalBox> TeethEditablePropertyBox;

	/** Reference to the box container of the current teeth editable property. */
	TSharedPtr<SMetaHumanCharacterEditorComboBox<EMetaHumanCharacterTeethPropertyType>> TeethEditablePropertyComboBox;

	/** Reference to the Eyelashes subtool view. */
	TSharedPtr<SVerticalBox> EyelashesSubToolView;

	/** Reference to the Teeth subtool view. */
	TSharedPtr<SVerticalBox> TeethSubToolView;
};
