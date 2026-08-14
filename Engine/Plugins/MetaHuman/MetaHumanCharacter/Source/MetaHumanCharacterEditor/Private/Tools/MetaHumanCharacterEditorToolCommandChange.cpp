// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorToolCommandChange.h"
#include "Editor/EditorEngine.h"

extern UNREALED_API UEditorEngine* GEditor;

void FMetaHumanCharacterEditorFaceEvaluationSettingsCommandChange::Apply(UObject* InObject)
{
	UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);
	GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>()->CommitFaceEvaluationSettings(Character, NewSettings);
	OnSettingsUpdateDelegate.ExecuteIfBound(ToolManager, NewSettings);
}

void FMetaHumanCharacterEditorFaceEvaluationSettingsCommandChange::Revert(UObject* InObject)
{
	UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);
	GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>()->CommitFaceEvaluationSettings(Character, OldSettings);
	OnSettingsUpdateDelegate.ExecuteIfBound(ToolManager, OldSettings);
}
