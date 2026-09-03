// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolChange.h"
#include "InteractiveToolManager.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorSubsystem.h"

/**
 * Tool Command change for undo/redo transactions.
 */
class FMetaHumanCharacterEditorToolCommandChange : public FToolCommandChange
{
public:
	FMetaHumanCharacterEditorToolCommandChange(TNotNull<UInteractiveToolManager*> InToolManager)
		: ToolManager{ InToolManager }
	{
	}

	//~Begin FToolCommandChange interface
	virtual bool HasExpired(UObject* InObject) const override
	{
		// If the ToolManager is not valid anymore it means the asset editor was closed so mark the transaction as expired
		return !ToolManager.IsValid();
	}
	//~End FToolCommandChange interface

protected:
	TWeakObjectPtr<UInteractiveToolManager> ToolManager;
};


DECLARE_DELEGATE_TwoParams(FOnSettingsUpdateDelegate, TWeakObjectPtr<UInteractiveToolManager>, const FMetaHumanCharacterFaceEvaluationSettings&);

/**
 * Base class for MetaHuman Character command change for undo/redo transactions.
 */
class FMetaHumanCharacterEditorFaceEvaluationSettingsCommandChange : public FMetaHumanCharacterEditorToolCommandChange
{
public:
	FMetaHumanCharacterEditorFaceEvaluationSettingsCommandChange(
		TNotNull<UMetaHumanCharacter*> InCharacter,
		const FMetaHumanCharacterFaceEvaluationSettings& InOldSettings,
		FOnSettingsUpdateDelegate InOnSettingsUpdateDelegate,
		TNotNull<UInteractiveToolManager*> InToolManager)
		: FMetaHumanCharacterEditorToolCommandChange(InToolManager)
		, OldSettings{ InOldSettings }
		, NewSettings{ InCharacter->FaceEvaluationSettings }
		, OnSettingsUpdateDelegate{ InOnSettingsUpdateDelegate }
	{
	}

	//~Begin FCommandChange interface
	virtual void Apply(UObject* InObject) override;
	virtual void Revert(UObject* InObject) override;
	//~End FCommandChange interface

private:
	FMetaHumanCharacterFaceEvaluationSettings OldSettings;
	FMetaHumanCharacterFaceEvaluationSettings NewSettings;

	FOnSettingsUpdateDelegate OnSettingsUpdateDelegate;
};
