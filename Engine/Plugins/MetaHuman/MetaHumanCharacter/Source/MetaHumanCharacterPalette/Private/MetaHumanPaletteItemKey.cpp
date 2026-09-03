// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPaletteItemKey.h"

#include "MetaHumanWardrobeItem.h"

FMetaHumanPaletteItemKey::FMetaHumanPaletteItemKey(const TSoftObjectPtr<UObject>& InPrincipalAsset, const FName& InVariation)
: Variation(InVariation)
, PrincipalAssetOrExternalWardrobeItem(InPrincipalAsset)
, bReferencesExternalWardrobeItem(false)
{
	// The item key must point to a self-contained asset, to ensure it's a stable reference
	check(!InPrincipalAsset.ToSoftObjectPath().IsSubobject());
}

FMetaHumanPaletteItemKey::FMetaHumanPaletteItemKey(const TSoftObjectPtr<UMetaHumanWardrobeItem>& InExternalWardrobeItem, const FName& InVariation)
: Variation(InVariation)
, PrincipalAssetOrExternalWardrobeItem(InExternalWardrobeItem)
, bReferencesExternalWardrobeItem(true)
{
	check(!InExternalWardrobeItem.ToSoftObjectPath().IsSubobject());
}

bool FMetaHumanPaletteItemKey::ReferencesExternalWardrobeItem() const
{
	// The value of bReferencesExternalWardrobeItem is meaningless if there is no asset or item 
	// referenced from this key, so return false in that case for consistency.
	return !PrincipalAssetOrExternalWardrobeItem.IsNull() && bReferencesExternalWardrobeItem;
}

bool FMetaHumanPaletteItemKey::TryGetPrincipalAsset(TSoftObjectPtr<UObject>& OutPrincipalAsset) const
{
	if (bReferencesExternalWardrobeItem)
	{
		OutPrincipalAsset.Reset();
		return false;
	}
	else
	{
		OutPrincipalAsset = PrincipalAssetOrExternalWardrobeItem;
		return true;
	}
}

bool FMetaHumanPaletteItemKey::TryGetExternalWardrobeItem(TSoftObjectPtr<UMetaHumanWardrobeItem>& OutWardrobeItem) const
{
	if (bReferencesExternalWardrobeItem)
	{
		OutWardrobeItem = PrincipalAssetOrExternalWardrobeItem.ToSoftObjectPath();
		return true;
	}
	else
	{
		OutWardrobeItem.Reset();
		return false;
	}
}

bool FMetaHumanPaletteItemKey::ReferencesSameAsset(const FMetaHumanPaletteItemKey& Other) const
{
	if (IsNull() || Other.IsNull())
	{
		return false;
	}

	if (PrincipalAssetOrExternalWardrobeItem != Other.PrincipalAssetOrExternalWardrobeItem)
	{
		return false;
	}

	// The two keys point to the same object, so they should agree on what type of object it is
	ensure(bReferencesExternalWardrobeItem == Other.bReferencesExternalWardrobeItem);
	return true;
}

bool FMetaHumanPaletteItemKey::IsNull() const
{
	return PrincipalAssetOrExternalWardrobeItem.IsNull();
}

void FMetaHumanPaletteItemKey::Reset()
{
	// Technically it should be enough to clear PrincipalAssetOrExternalWardrobeItem, but for
	// maximum robustness we clear all variables.
	*this = FMetaHumanPaletteItemKey();
}

FString FMetaHumanPaletteItemKey::ToAssetNameString() const
{
	const FString PrincipalAssetName = PrincipalAssetOrExternalWardrobeItem.IsNull() ? TEXT("NoAsset") : PrincipalAssetOrExternalWardrobeItem.GetAssetName();

	if (Variation != NAME_None)
	{
		return PrincipalAssetName + TEXT("_") + Variation.ToString();
	}
	else
	{
		return PrincipalAssetName;
	}
}

FString FMetaHumanPaletteItemKey::ToDebugString() const
{
	return FString::Format(TEXT("(Asset={0},Variation=\"{1}\")"), { PrincipalAssetOrExternalWardrobeItem.ToString(), Variation.ToString() });
}

bool operator==(const FMetaHumanPaletteItemKey& A, const FMetaHumanPaletteItemKey& B)
{
	if (A.PrincipalAssetOrExternalWardrobeItem != B.PrincipalAssetOrExternalWardrobeItem)
	{
		return false;
	}

	// Keys point to the same asset

	if (A.PrincipalAssetOrExternalWardrobeItem.IsNull())
	{
		// Both keys are null. Variation is irrelevant.
		return true;
	}

	// This should match, since the keys reference the same asset
	ensure(A.bReferencesExternalWardrobeItem == B.bReferencesExternalWardrobeItem);

	return A.Variation == B.Variation;
}

bool operator!=(const FMetaHumanPaletteItemKey& A, const FMetaHumanPaletteItemKey& B)
{
	return !(A == B);
}

bool operator<(const FMetaHumanPaletteItemKey& A, const FMetaHumanPaletteItemKey& B)
{
	if (A.PrincipalAssetOrExternalWardrobeItem.ToSoftObjectPath().FastLess(B.PrincipalAssetOrExternalWardrobeItem.ToSoftObjectPath()))
	{
		return true;
	}

	return A.PrincipalAssetOrExternalWardrobeItem == B.PrincipalAssetOrExternalWardrobeItem
		&& A.Variation.CompareIndexes(B.Variation) < 0;
}
