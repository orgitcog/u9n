// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "SMetaHumanCharacterEditorToolView.h"
#include "UI/Widgets/SMetaHumanCharacterEditorParametricView.h"

class UMetaHumanCharacterEditorToolWithSubTools;
class SMetaHumanCharacterEditorFixedCompatibilityPanel;

/** View for displaying the Body Model Tool in the MetaHumanCharacter editor */
class SMetaHumanCharacterEditorBodyModelToolView
	: public SMetaHumanCharacterEditorToolView
	, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorToolWithSubTools* InTool);

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
	/** Gets the body model parametric subtool properties. */
	UInteractiveToolPropertySet* GetParametricProperties() const;

	/** Gets the body model fixed compatibility subtool properties. */
	UInteractiveToolPropertySet* GetFixedCompatibilityProperties() const ;

	/** Makes the parametric subtool view. */
	void MakeParametricSubToolView();

	/** Makes the parametric subtool warning view. */
	void MakeParametricFixedWarningView();

	/** Makes the fixed compatibility subtool view. */
	void MakeFixedCompatibilitySubToolView();

	/** Makes the fixed compatibility subtool warning view. */
	void MakeFixedCompatibilityWarningView();

	/** Called when the property set of this tool has been modified. */
	void OnPropertySetsModified();

	/** Creates the section widget for showing the body parametric properties. */
	TSharedRef<SWidget> CreateParametricSubToolViewSection();

	/* Creates the panel widget for an array of constraint names. */
	TSharedRef<SWidget> CreateParametricConstraintsPanel(const FText& Label, const TArray<FName>& ConstraintNames, bool bDiagnosticsView = false);

	/** 
	 * Called when the user starts dragging a constraint slider. 
	 * 
	 * When they release the slider, OnParametricConstraintsChanged will be called with bInCommit = true.
	 */
	void OnBeginConstraintEditing();

	/** On parametric constraints changed */
	void OnParametricConstraintsChanged(bool bInCommit);

	/** On reset parametric clicked */
	FReply OnResetButtonClicked() const;

	/** On perform parametric fit clicked */
	FReply OnPerformParametricFitButtonClicked() const;

	/** Creates the section widget for showing the fixed compatibility properties. */
	TSharedRef<SWidget> CreateFixedCompatibilitySubToolViewSection();

	/** Gets the visibility for the parametric subtool view. */
	EVisibility GetParametricSubToolViewVisibility() const;

	/** Gets the visibility for the parametric fixed warning. */
	EVisibility GetParametricSubToolFixedWarningVisibility() const;

	/** Gets the visibility for the fixed compatibility subtool view. */
	EVisibility GetFixedCompatibilitySubToolViewVisibility() const;

	/** Gets the visibility for the fixed compatibility warning about no optional content. */
	EVisibility GetFixedCompatibilitySubToolWarningVisibility() const;

	/** Reference to the parametric subtool view. */
	TSharedPtr<SVerticalBox> ParametricSubToolView;

	/** Reference to the parametric fixed warning view. */
	TSharedPtr<SVerticalBox> ParametricFixedWarningView;

	/** Reference to the fixed compatibility subtool view. */
	TSharedPtr<SVerticalBox> FixedCompatibilitySubToolView;

	/** Reference to the fixed compatibilit warning view. */
	TSharedPtr<SVerticalBox> FixedCompatibilityWarningView;

	/** Reference to the fixed compatibility panel. */
	TSharedPtr<SMetaHumanCharacterEditorFixedCompatibilityPanel> FixedCompatibilityPanel;
};
