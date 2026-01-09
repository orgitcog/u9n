// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "SMetaHumanCharacterEditorToolView.h"


class FReply;
class IDetailsView;
class UMetaHumanCharacterEditorToolWithSubTools;

/** View for displaying the Conform Tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorConformToolView
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
	/** Creates the section widget for showing the Warning panel. */
	TSharedRef<SWidget> CreateConformToolViewWarningSection();

	/** Creates the section widget for showing the Import properties. */
	TSharedRef<SWidget> CreateConformToolViewImportSection();

	/** Called when the property set of this tool has been modified. */
	void OnPropertySetsModified();

	/** True if the Import button is enabled. */
	bool IsImportButtonEnabled() const;

	/** Called when the Import button is clicked. */
	FReply OnImportButtonClicked();

	/** Gets the displayed text of the Import button. */
	FText GetImportButtonText() const;

	/** Gets the visibility of the warning panel */
	EVisibility GetWarningVisibility() const;

	/** Gets the text for the warning panel */
	FText GetWarning() const;

	/** Reference to the Details View used for showing import properties. */
	TSharedPtr<IDetailsView> DetailsView;
};
