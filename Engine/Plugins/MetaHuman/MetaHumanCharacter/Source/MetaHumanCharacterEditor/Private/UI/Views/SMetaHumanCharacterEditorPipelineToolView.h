// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "SMetaHumanCharacterEditorToolView.h"

class FReply;
class IDetailsView;
class UMetaHumanCharacterEditorPipelineTool;

/** View for displaying the Pipeline Tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorPipelineToolView
	: public SMetaHumanCharacterEditorToolView
	, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorToolView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorPipelineTool* InTool);

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
	/** Creates the section widget for showing the main Details View. */
	TSharedRef<SWidget> CreatePipelineToolViewDetailsViewSection();

	/** Creates the section widget for showing the Assemble button. */
	TSharedRef<SWidget> CreatePipelineToolViewAssembleSection();

	/** True if the Assemble button is enabled. */
	bool IsAssembleButtonEnabled() const;

	/** Called when the Assemble button is clicked. */
	FReply OnAssembleButtonClicked();

	/** Gets the Assemble button label text. */
	FText GetAssembleButtonText() const;

	/** Gets the warning message visibility. */
	EVisibility GetWarningVisibility() const;

	/** The optional error message displayed when building the pipeline. */
	FText BuildErrorMsg;

	/** Reference to the Details View that displays pipeline properties. */
	TSharedPtr<IDetailsView> DetailsView;
};
