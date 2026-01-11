// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterPaletteItem.h"

#include "MetaHumanPaletteItemKey.h"
#include "MetaHumanWardrobeItem.h"

FMetaHumanPaletteItemKey FMetaHumanCharacterPaletteItem::GetItemKey() const
{
	// If there's no wardrobe item, this whole palette item is considered null and should have a
	// null key.
	if (!WardrobeItem)
	{
		return FMetaHumanPaletteItemKey();
	}

	// The item key needs to reference a self-contained asset.
	//
	// If the Wardrobe Item is external, that can be the reference, otherwise we use the item's 
	// principal asset.
	if (WardrobeItem->IsExternal())
	{
		return FMetaHumanPaletteItemKey(TSoftObjectPtr<UMetaHumanWardrobeItem>(WardrobeItem), Variation);
	}
	else
	{
		return FMetaHumanPaletteItemKey(WardrobeItem->PrincipalAsset, Variation);
	}
}

FText FMetaHumanCharacterPaletteItem::GetOrGenerateDisplayName() const
{
#if WITH_EDITORONLY_DATA
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
#endif // WITH_EDITORONLY_DATA

	if (!WardrobeItem)
	{
		return NSLOCTEXT("MetaHumanCharacterPalette", "NullPaletteItemDisplayName", "(Empty Item)");
	}

	// No user-defined display name, so generate one from asset name and variation, if any.

	if (Variation == NAME_None)
	{
		return FText::FromString(WardrobeItem->GetName());
	}

	FString VariationString;
	if (Variation.GetComparisonIndex() == FName(NAME_None).GetComparisonIndex())
	{
		// The text part of the name is empty but there's a valid number, so just display the
		// number.
		//
		// Otherwise, the string would be "None_3" if the number were 3, for example.
		VariationString = LexToString(Variation.GetNumber());
	}
	else if (Variation.GetNumber() == NAME_NO_NUMBER)
	{
		// There's a valid text part, but no number
		VariationString = Variation.GetPlainNameString();
	}
	else
	{
		// There's a valid text part and a number
		//
		// Concatenate them with a space instead of an underscore, as it looks nicer.
		VariationString = FString::Format(TEXT("{0} {1}"), { Variation.GetPlainNameString(), Variation.GetNumber() });
	}

	return FText::FromString(FString::Format(TEXT("{0} ({1})"), { WardrobeItem->GetName(), VariationString }));
}

UObject* FMetaHumanCharacterPaletteItem::LoadPrincipalAssetSynchronous() const
{
	return WardrobeItem ? WardrobeItem->PrincipalAsset.LoadSynchronous() : nullptr;
}
