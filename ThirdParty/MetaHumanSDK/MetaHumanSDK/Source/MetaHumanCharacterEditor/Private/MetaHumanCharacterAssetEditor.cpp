// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterAssetEditor.h"
#include "MetaHumanCharacterEditorToolkit.h"
#include "MetaHumanCharacter.h"

void UMetaHumanCharacterAssetEditor::GetObjectsToEdit(TArray<UObject*>& OutObjectsToEdit)
{
	OutObjectsToEdit.Add(ObjectToEdit);
}

TSharedPtr<FBaseAssetToolkit> UMetaHumanCharacterAssetEditor::CreateToolkit()
{
	return MakeShared<FMetaHumanCharacterEditorToolkit>(this);
}

UMetaHumanCharacter* UMetaHumanCharacterAssetEditor::GetObjectToEdit() const
{
	return ObjectToEdit;
}

void UMetaHumanCharacterAssetEditor::SetObjectToEdit(UMetaHumanCharacter* InObjectToEdit)
{
	ObjectToEdit = InObjectToEdit;
}