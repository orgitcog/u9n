// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "SMetaHumanCharacterEditorToolView.h"


class FReply;
class UMetaHumanCharacterEditorFaceSculptTool;
class UMetaHumanCharacterEditorFaceMoveTool;

/** View for displaying the face scult tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorFaceToolView
	: public SMetaHumanCharacterEditorToolView
	, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorFaceToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorFaceSculptTool* InTool);

protected:
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual UInteractiveToolPropertySet* GetToolProperties() const override;
	//~ End of SMetaHumanCharacterEditorToolView interface
	
	//~ Begin FNotifyHook interface
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	//~ End of FNotifyHook interface

protected:
	/** Creates the section widget for showing the manipulator properties. */
	TSharedRef<SWidget> CreateManipulatorsViewSection();

	/** Creates the section widget for showing the head parameter properties. */
	TSharedRef<SWidget> CreateHeadParametersViewSection();

private:

	/** On reset clicked */
	FReply OnResetButtonClicked() const;

	/** On reset neck clicked */
	FReply OnResetNeckButtonClicked() const;
};

/** View for displaying the face sculpt tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorFaceSculptToolView
	: public SMetaHumanCharacterEditorFaceToolView
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorFaceSculptToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorFaceSculptTool* InTool);

protected:
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual void MakeToolView() override;
	//~ End of SMetaHumanCharacterEditorToolView interface

private:
};


/** View for displaying the face move tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorFaceMoveToolView
	: public SMetaHumanCharacterEditorFaceToolView
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorFaceMoveToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UMetaHumanCharacterEditorFaceMoveTool* InTool);

protected:
	//~ Begin SMetaHumanCharacterEditorToolView interface
	virtual void MakeToolView() override;
	//~ End of SMetaHumanCharacterEditorToolView interface

private:
	TSharedRef<SWidget> CreateGizmoSelectionSection();

};

