// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterPalette.h"

#include "MetaHumanCharacterEditorPipeline.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterPaletteLog.h"
#include "MetaHumanCharacterPaletteProjectSettings.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanItemPipeline.h"
#include "MetaHumanPaletteItemPath.h"
#include "MetaHumanWardrobeItem.h"
#include "MetaHumanCollectionPipeline.h"

#include "Logging/StructuredLog.h"

#if WITH_EDITORONLY_DATA
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#endif

bool FMetaHumanPaletteBuiltData::HasBuildOutputForItem(const FMetaHumanPaletteItemPath& ItemPath) const
{
	// Items must produce their own build output
	const FMetaHumanPipelineBuiltData* BuiltData = ItemBuiltData.Find(ItemPath);
	if (!BuiltData)
	{
		return false;
	}

	return BuiltData->BuildOutput.IsValid();
}

bool FMetaHumanPaletteBuiltData::ContainsOnlyValidBuildOutputForItem(const FMetaHumanPaletteItemPath& ItemPath) const
{
	if (!ItemBuiltData.Contains(ItemPath))
	{
		// Items must produce build output for themselves
		return false;
	}

	for (const TPair<FMetaHumanPaletteItemPath, FMetaHumanPipelineBuiltData>& Pair : ItemBuiltData)
	{
		if (!Pair.Key.IsEqualOrChildPathOf(ItemPath))
		{
			// Items may not produce build output for items outside of their own path
			return false;
		}

		if (Pair.Value.SlotName == NAME_None
			&& Pair.Key != ItemPath)
		{
			// Invalid slot name for item
			//
			// The base item is allowed to have an empty slot name, because the item pipeline
			// doesn't know which slot the item is in.
			return false;
		}

		if (!Pair.Value.BuildOutput.IsValid())
		{
			// Build output struct must be valid
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR

void FMetaHumanPaletteBuiltData::IntegrateItemBuiltData(const FMetaHumanPaletteItemPath& SourceItemPath, FName SourceItemSlotName, FMetaHumanPaletteBuiltData&& SourceItemBuiltData)
{
	check(SourceItemBuiltData.ContainsOnlyValidBuildOutputForItem(SourceItemPath));

	FMetaHumanPipelineBuiltData& ItemOwnBuiltData = SourceItemBuiltData.ItemBuiltData.FindChecked(SourceItemPath);
	// The item pipeline doesn't know which slot the item is in, so we write that here.
	ItemOwnBuiltData.SlotName = SourceItemSlotName;

	ItemBuiltData.Append(MoveTemp(SourceItemBuiltData.ItemBuiltData));
}

bool UMetaHumanCharacterPalette::TryAddItem(const FMetaHumanCharacterPaletteItem& NewItem)
{
	if (ContainsItem(NewItem.GetItemKey()))
	{
		return false;
	}

	Items.Add(NewItem);
	return true;
}

bool UMetaHumanCharacterPalette::TryAddItemFromPrincipalAsset(FName SlotName, const FSoftObjectPath& PrincipalAsset, FMetaHumanPaletteItemKey& OutNewItemKey)
{
	const UObject* LoadedPrincipalAsset = PrincipalAsset.TryLoad();

	if (!LoadedPrincipalAsset
		|| !GetPaletteEditorPipeline()
		|| !GetPaletteEditorPipeline()->IsPrincipalAssetClassCompatibleWithSlot(SlotName, LoadedPrincipalAsset->GetClass()))
	{
		// Asset doesn't exist, or the slot doesn't support this asset type
		return false;
	}

	UMetaHumanWardrobeItem* WardrobeItem = NewObject<UMetaHumanWardrobeItem>(this);
	WardrobeItem->PrincipalAsset = PrincipalAsset;

	AddItemFromKnownCompatibleWardrobeItem(SlotName, WardrobeItem, OutNewItemKey);
	return true;
}

bool UMetaHumanCharacterPalette::TryAddItemFromWardrobeItem(FName SlotName, TNotNull<UMetaHumanWardrobeItem*> WardrobeItem, FMetaHumanPaletteItemKey& OutNewItemKey)
{
	if (!WardrobeItem->IsExternal())
	{
		// Palettes can't directly reference Wardrobe Items that belong to other assets
		return false;
	}

	if (!GetPaletteEditorPipeline()
		|| !GetPaletteEditorPipeline()->IsWardrobeItemCompatibleWithSlot(SlotName, WardrobeItem))
	{
		// The slot doesn't support this asset type
		return false;
	}

	AddItemFromKnownCompatibleWardrobeItem(SlotName, WardrobeItem, OutNewItemKey);
	return true;
}

bool UMetaHumanCharacterPalette::TryRemoveItem(const FMetaHumanPaletteItemKey& ExistingKey)
{
	const int32 ExistingIndex = Items.IndexOfByPredicate([&ExistingKey](const FMetaHumanCharacterPaletteItem& ExistingItem)
		{
			return ExistingItem.GetItemKey() == ExistingKey;
		});

	if (ExistingIndex == INDEX_NONE)
	{
		// Key not found in items array
		return false;
	}

	Items.RemoveAt(ExistingIndex);

	return true;
}

bool UMetaHumanCharacterPalette::TryReplaceItem(const FMetaHumanPaletteItemKey& ExistingKey, const FMetaHumanCharacterPaletteItem& NewItem)
{
	const int32 ExistingIndex = Items.IndexOfByPredicate([&ExistingKey](const FMetaHumanCharacterPaletteItem& ExistingItem)
		{
			return ExistingItem.GetItemKey() == ExistingKey;
		});

	if (ExistingIndex == INDEX_NONE)
	{
		// Key not found in items array
		return false;
	}

	const FMetaHumanPaletteItemKey NewKey = NewItem.GetItemKey();
	if (NewKey != ExistingKey && ContainsItem(NewKey))
	{
		// Can't change the item key to one that already exists
		return false;
	}

	Items[ExistingIndex] = NewItem;
	return true;
}

int32 UMetaHumanCharacterPalette::RemoveAllItemsForSlot(FName SlotName)
{
	return Items.RemoveAll([SlotName](const FMetaHumanCharacterPaletteItem& Item)
		{
			return Item.SlotName == SlotName;
		});
}

FName UMetaHumanCharacterPalette::GenerateUniqueVariationName(const FMetaHumanPaletteItemKey& SourceKey) const
{
	// Items that have the same principal asset as the passed in key
	TArray<FName, TInlineAllocator<8>> MatchingItemVariations;

	bool bFoundExactMatch = false;
	for (const FMetaHumanCharacterPaletteItem& ExistingItem : Items)
	{
		const FMetaHumanPaletteItemKey ExistingKey = ExistingItem.GetItemKey();
		if (ExistingKey.ReferencesSameAsset(SourceKey))
		{
			MatchingItemVariations.Add(ExistingKey.Variation);

			if (ExistingKey == SourceKey)
			{
				bFoundExactMatch = true;
			}
		}
	}

	if (!bFoundExactMatch)
	{
		// SourceKey doesn't match any existing items
		return SourceKey.Variation;
	}

	// Find a variation name that doesn't conflict with an existing item

	// Start generating variations at 2, so that we get "Asset", "Asset 2", "Asset 3", etc as 
	// generated names, without using "Asset 1".
	FName NewVariation = SourceKey.Variation;
	if (NewVariation.GetNumber() == 0)
	{
		NewVariation.SetNumber(1);
	}

	const int32 OriginalVariationNumber = FMath::Clamp(NewVariation.GetNumber(), 0, MAX_int32);
	int32 VariationNumber = OriginalVariationNumber;
	while (true)
	{
		// Keep the number non-negative
		if (VariationNumber == MAX_int32)
		{
			VariationNumber = 0;
		}

		VariationNumber++;

		if (VariationNumber == OriginalVariationNumber)
		{
			// We have tried all possible variations and failed to find an unused one.
			//
			// Given that the variation number is 32 bits, this should never happen in practice.
			checkNoEntry();

			// If continuing past the assert, return the original name just to avoid an infinite 
			// loop.
			return SourceKey.Variation;
		}

		NewVariation.SetNumber(VariationNumber);

		if (!MatchingItemVariations.Contains(NewVariation))
		{
			break;
		}
	}

	return NewVariation;
}
#endif // WITH_EDITOR

bool UMetaHumanCharacterPalette::ContainsItem(const FMetaHumanPaletteItemKey& Key) const
{
	return Items.ContainsByPredicate([&Key](const FMetaHumanCharacterPaletteItem& Item)
	{
		return Item.GetItemKey() == Key;
	});
}

bool UMetaHumanCharacterPalette::TryFindItem(const FMetaHumanPaletteItemKey& Key, FMetaHumanCharacterPaletteItem& OutItem) const
{
	const FMetaHumanCharacterPaletteItem* FoundItem = Items.FindByPredicate([&Key](const FMetaHumanCharacterPaletteItem& Item)
		{
			return Item.GetItemKey() == Key;
		});

	if (FoundItem == nullptr)
	{
		// Item not found in items array
		return false;
	}

	OutItem = *FoundItem;
	return true;
}

bool UMetaHumanCharacterPalette::TryResolveItem(const FMetaHumanPaletteItemPath& ItemPath, const UMetaHumanCharacterPalette*& OutContainingPalette, FMetaHumanCharacterPaletteItem& OutItem) const
{
	OutContainingPalette = nullptr;
	OutItem = FMetaHumanCharacterPaletteItem();

	if (ItemPath.IsEmpty())
	{
		return false;
	}

	if (ItemPath.GetNumPathEntries() > 1)
	{
		// TODO: Support nested palettes
		unimplemented();
		return false;
	}

	const FMetaHumanPaletteItemKey ItemKey = ItemPath.GetPathEntry(0);
	if (!TryFindItem(ItemKey, OutItem))
	{
		return false;
	}

	OutContainingPalette = this;
	return true;
}

bool UMetaHumanCharacterPalette::TryResolvePipeline(const FMetaHumanPaletteItemPath& ItemPath, const UMetaHumanCharacterPipeline*& OutPipeline) const
{
	OutPipeline = nullptr;

	if (ItemPath.IsEmpty())
	{
		OutPipeline = GetPalettePipeline();
		return OutPipeline != nullptr;
	}

	const UMetaHumanCharacterPalette* ContainingPalette = nullptr;
	FMetaHumanCharacterPaletteItem Item;
	if (!TryResolveItem(ItemPath, ContainingPalette, Item))
	{
		return false;
	}

	if (!Item.WardrobeItem)
	{
		return false;
	}

	OutPipeline = Item.WardrobeItem->GetPipeline();

	if (OutPipeline == nullptr)
	{
		if (const UMetaHumanCollectionPipeline* CollectionPipeline = Cast<UMetaHumanCollectionPipeline>(GetPalettePipeline()))
		{
			if (UObject* PrincipalAsset = Item.LoadPrincipalAssetSynchronous())
			{
				OutPipeline = CollectionPipeline->GetFallbackItemPipelineForAssetType(PrincipalAsset->GetClass());
			}
		}
	}

	return OutPipeline != nullptr;
}

bool UMetaHumanCharacterPalette::TryResolveItemPipeline(const FMetaHumanPaletteItemPath& ItemPath, const UMetaHumanItemPipeline*& OutPipeline) const
{
	const UMetaHumanCharacterPipeline* CharacterPipeline = nullptr;
	if (!TryResolvePipeline(ItemPath, CharacterPipeline))
	{
		return false;
	}

	OutPipeline = Cast<UMetaHumanItemPipeline>(CharacterPipeline);
	return OutPipeline != nullptr;
}

#if WITH_EDITOR
void UMetaHumanCharacterPalette::AddItemFromKnownCompatibleWardrobeItem(FName SlotName, TNotNull<UMetaHumanWardrobeItem*> WardrobeItem, FMetaHumanPaletteItemKey& OutNewItemKey)
{
	OutNewItemKey.Reset();

	TSharedRef<FMetaHumanCharacterPaletteItem> NewItem = MakeShared<FMetaHumanCharacterPaletteItem>();
	NewItem->WardrobeItem = WardrobeItem;
	NewItem->SlotName = SlotName;

	// Ensure new item key is unique
	NewItem->Variation = GenerateUniqueVariationName(NewItem->GetItemKey());
	
	// The add operation should succeed because the key should be unique
	verify(TryAddItem(NewItem.Get()));

	OutNewItemKey = NewItem->GetItemKey();
}
#endif // WITH_EDITOR
