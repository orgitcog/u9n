// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPtr.h"

#include "MetaHumanPaletteItemKey.generated.h"

class UMetaHumanWardrobeItem;

/** Uniquely identifies an item in a UMetaHumanCharacterPalette */
USTRUCT(BlueprintType)
struct METAHUMANCHARACTERPALETTE_API FMetaHumanPaletteItemKey
{
	GENERATED_BODY()

public:
	FMetaHumanPaletteItemKey() = default;
	explicit FMetaHumanPaletteItemKey(const TSoftObjectPtr<UObject>& InPrincipalAsset, const FName& InVariation);
	explicit FMetaHumanPaletteItemKey(const TSoftObjectPtr<UMetaHumanWardrobeItem>& InExternalWardrobeItem, const FName& InVariation);

	/** Returns true if the Wardrobe Item referenced by this palette item is a self-contained asset */
	bool ReferencesExternalWardrobeItem() const;

	/** 
	 * If this item references a Principal Asset *directly*, this function succeeds and returns it
	 * via the given argument. 
	 * 
	 * Otherwise, it will fail and return nothing.
	 */
	[[nodiscard]] bool TryGetPrincipalAsset(TSoftObjectPtr<UObject>& OutPrincipalAsset) const;

	/** 
	 * If this item references an external Wardrobe Item, this function succeeds and returns it
	 * via the given argument. 
	 * 
	 * Otherwise, it will fail and return nothing.
	 */
	[[nodiscard]] bool TryGetExternalWardrobeItem(TSoftObjectPtr<UMetaHumanWardrobeItem>& OutWardrobeItem) const;

	/** Returns true if the other key is identical to this one except for Variation */
	bool ReferencesSameAsset(const FMetaHumanPaletteItemKey& Other) const;

	/** 
	 * Returns false if this key *could* represent an item. This does not guarantee that the item 
	 * exists in any Palette.
	 * 
	 * If this returns true, this key represents the null item and can be used to specify that a
	 * slot should not have any item selected, for example.
	 */
	bool IsNull() const;

	/** Resets the key to the null state */
	void Reset();

	/**
	 * Produces a string suitable for using as part of an asset name
	 *
	 * Not guaranteed to be unique within the owning Palette, e.g. if you call this on every item
	 * in the Palette, it may return the same string for multiple items.
	 */
	FString ToAssetNameString() const;

	/** Produces a string with the contents of this key, suitable for log messages, etc */
	FString ToDebugString() const;

	friend uint32 GetTypeHash(const FMetaHumanPaletteItemKey& ItemKey)
	{
		return HashCombineFast(GetTypeHash(ItemKey.PrincipalAssetOrExternalWardrobeItem), GetTypeHash(ItemKey.Variation));
	}

	METAHUMANCHARACTERPALETTE_API friend bool operator==(const FMetaHumanPaletteItemKey& A, const FMetaHumanPaletteItemKey& B);
	METAHUMANCHARACTERPALETTE_API friend bool operator!=(const FMetaHumanPaletteItemKey& A, const FMetaHumanPaletteItemKey& B);
	// A fast less-than operator that is not guaranteed to return the same result across engine instances.
	METAHUMANCHARACTERPALETTE_API friend bool operator<(const FMetaHumanPaletteItemKey& A, const FMetaHumanPaletteItemKey& B);

	/** 
	 * A name used to disambiguate items that reference the same asset or wardrobe item.
	 * 
	 * This is expected to be NAME_None in most cases.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Pipeline")
	FName Variation;

private:
	UPROPERTY(VisibleAnywhere, Category = "Pipeline", meta=(Untracked))
	TSoftObjectPtr<UObject> PrincipalAssetOrExternalWardrobeItem;

	UPROPERTY(VisibleAnywhere, Category = "Pipeline")
	bool bReferencesExternalWardrobeItem = false;
};
