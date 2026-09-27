// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanPaletteItemPath.h"

#include "MetaHumanCharacterPaletteItem.generated.h"

struct FMetaHumanPaletteItemKey;
class UMetaHumanCharacterPalette;
class UMetaHumanCharacterPipeline;
class UMetaHumanWardrobeItem;

USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanCharacterPaletteItem
{
	GENERATED_BODY()

public:
	/** Return a key for this item that must be unique within its containing palette */
	FMetaHumanPaletteItemKey GetItemKey() const;

	/** 
	 * Return a friendly name that can be displayed in the UI
	 * 
	 * If DisplayName is set, this just returns DisplayName, otherwise it will use the other 
	 * properties to generate a name.
	 */
	FText GetOrGenerateDisplayName() const;

	/** Convenience function for calling LoadSynchronous on the Wardrobe Item's principal asset */
	UObject* LoadPrincipalAssetSynchronous() const;

	/** 
	 * The Wardrobe Item that this item represents.
	 * 
	 * Note that this could be its own asset or a subobject of a MetaHuman Collection.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Character")
	TObjectPtr<class UMetaHumanWardrobeItem> WardrobeItem;

	/** A name used to disambiguate items that have the same WardrobeItem */
	UPROPERTY(VisibleAnywhere, Category = "Character")
	FName Variation;

	/** The slot that this item targets */
	UPROPERTY(VisibleAnywhere, Category = "Character")
	FName SlotName;

#if WITH_EDITORONLY_DATA

	/** An optional display name to use in the editor UI */
	UPROPERTY(EditAnywhere, Category = "Character")
	FText DisplayName;

#endif // WITH_EDITORONLY_DATA
};
