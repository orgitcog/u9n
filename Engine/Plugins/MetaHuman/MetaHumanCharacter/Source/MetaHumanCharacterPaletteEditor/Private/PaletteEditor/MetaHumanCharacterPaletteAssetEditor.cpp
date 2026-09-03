// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaletteEditor/MetaHumanCharacterPaletteAssetEditor.h"
#include "PaletteEditor/MetaHumanCharacterPaletteEditorToolkit.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCollection.h"

void UMetaHumanCharacterPaletteAssetEditor::GetObjectsToEdit(TArray<UObject*>& OutObjectsToEdit)
{
	if (bIsPaletteEditable)
	{
		OutObjectsToEdit.Add(Collection);
	}
	else
	{
		OutObjectsToEdit.Add(CharacterInstance);
	}
}

TSharedPtr<FBaseAssetToolkit> UMetaHumanCharacterPaletteAssetEditor::CreateToolkit()
{
	return MakeShared<FMetaHumanCharacterPaletteEditorToolkit>(this);
}

void UMetaHumanCharacterPaletteAssetEditor::SetObjectToEdit(UMetaHumanCollection* InObject)
{
	check(InObject);

	Collection = InObject;
	CharacterInstance = Collection->GetMutableDefaultInstance();

	bIsPaletteEditable = true;
}

void UMetaHumanCharacterPaletteAssetEditor::SetObjectToEdit(UMetaHumanCharacterInstance* InObject)
{
	check(InObject);

	CharacterInstance = InObject;
	Collection = CharacterInstance->GetMetaHumanCollection();
	// It's possible for an Instance to be created with a null Collection, but callers should not
	// try to open this asset editor on an Instance that's in that state.
	check(Collection);

	bIsPaletteEditable = false;
}
