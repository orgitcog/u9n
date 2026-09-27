// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterPaletteItem.h"
#include "UObject/Object.h"

#include "MetaHumanCharacterPaletteItemWrapper.generated.h"

/**
 * A UObject wrapper for FMetaHumanCharacterPaletteItem, so that it can be edited in a 
 * Details panel.
 */
UCLASS()
class UMetaHumanCharacterPaletteItemWrapper : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Character", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterPaletteItem Item;
};
