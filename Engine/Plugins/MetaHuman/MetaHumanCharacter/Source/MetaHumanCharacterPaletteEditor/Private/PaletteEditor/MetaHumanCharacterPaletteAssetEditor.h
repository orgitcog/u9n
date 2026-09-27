// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UAssetEditor.h"

#include "MetaHumanCharacterPaletteAssetEditor.generated.h"

class UMetaHumanCharacterInstance;
class UMetaHumanCollection;

/**
 * An asset editor capable of editing Character Palette and Character Instance assets
 */
UCLASS()
class UMetaHumanCharacterPaletteAssetEditor : public UAssetEditor
{
	GENERATED_BODY()

public:

	// Begin UAssetEditor Interface
	virtual void GetObjectsToEdit(TArray<UObject*>& OutObjectsToEdit) override;
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;
	// End UAssetEditor Interface

	UMetaHumanCollection* GetMetaHumanCollection() const
	{
		return Collection;
	}

	UMetaHumanCharacterInstance* GetCharacterInstance() const
	{
		return CharacterInstance;
	}

	bool IsPaletteEditable() const
	{
		return bIsPaletteEditable;
	}

	/** The Collection is the object being edited. CharacterInstance will be the Collection's default instance. */
	void SetObjectToEdit(UMetaHumanCollection* InObject);
	/** The Instance is the object being edited. Its Palette will be accessible but not editable. */
	void SetObjectToEdit(UMetaHumanCharacterInstance* InObject);

private:
	UPROPERTY()
	TObjectPtr<UMetaHumanCollection> Collection;

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterInstance> CharacterInstance;

	bool bIsPaletteEditable;
};
