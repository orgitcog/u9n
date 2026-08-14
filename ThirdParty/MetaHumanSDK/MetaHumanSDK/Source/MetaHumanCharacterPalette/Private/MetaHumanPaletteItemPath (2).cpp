// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPaletteItemPath.h"

#include "Algo/NoneOf.h"

const FMetaHumanPaletteItemPath FMetaHumanPaletteItemPath::Collection;

FMetaHumanPaletteItemPath::FMetaHumanPaletteItemPath(const FMetaHumanPaletteItemKey& InItem)
	: Item(InItem)
{
}

FMetaHumanPaletteItemPath::FMetaHumanPaletteItemPath(const TArray<FMetaHumanPaletteItemKey>& InParentItems, const FMetaHumanPaletteItemKey& InItem)
	: ParentItems(InParentItems)
	, Item(InItem)
{
	check(Algo::NoneOf(InParentItems, &FMetaHumanPaletteItemKey::IsNull));
}

FMetaHumanPaletteItemPath::FMetaHumanPaletteItemPath(const FMetaHumanPaletteItemPath& InParentItemPath, const FMetaHumanPaletteItemKey& InItem)
	: Item(InItem)
{
	// Populate ParentItems only if it would be meaningful
	if (!InParentItemPath.IsEmpty()
		&& !InItem.IsNull())
	{
		ParentItems.Append(InParentItemPath.ParentItems);
		ParentItems.Add(InParentItemPath.Item);
	}
}

bool FMetaHumanPaletteItemPath::IsEmpty() const
{
	// If the path is non-empty, the last item in the path (stored in Item) must be valid.
	return Item.IsNull();
}

int32 FMetaHumanPaletteItemPath::GetNumPathEntries() const
{
	if (Item.IsNull())
	{
		return 0;
	}

	return ParentItems.Num() + 1;
}

FMetaHumanPaletteItemKey FMetaHumanPaletteItemPath::GetPathEntry(int32 Index) const
{
	check(!Item.IsNull());
	check(Index >= 0);
	check(Index < GetNumPathEntries());

	if (Index == ParentItems.Num())
	{
		return Item;
	}
	
	return ParentItems[Index];
}

bool FMetaHumanPaletteItemPath::IsDirectChildPathOf(const FMetaHumanPaletteItemPath& ParentPath) const
{
	if (GetNumPathEntries() != ParentPath.GetNumPathEntries() + 1)
	{
		return false;
	}

	if (ParentPath.IsEmpty())
	{
		// This must be a path with no parent items, so there are no entries to compare.
		return true;
	}

	for (int32 Index = 0; Index < ParentPath.ParentItems.Num(); Index++)
	{
		if (ParentPath.ParentItems[Index] != ParentItems[Index])
		{
			return false;
		}
	}

	return ParentItems.Last() == ParentPath.Item;
}

bool FMetaHumanPaletteItemPath::IsEqualOrChildPathOf(const FMetaHumanPaletteItemPath& ParentPath) const
{
	if (ParentPath.IsEmpty())
	{
		// All paths are equal to or children of the empty path
		return true;
	}

	if (ParentPath.GetNumPathEntries() > GetNumPathEntries())
	{
		// Parent path is longer, so it can't be equal or a parent of this path
		return false;
	}

	for (int32 Index = 0; Index < ParentPath.GetNumPathEntries(); Index++)
	{
		if (ParentPath.GetPathEntry(Index) != GetPathEntry(Index))
		{
			return false;
		}
	}

	return true;
}

void FMetaHumanPaletteItemPath::Append(const FMetaHumanPaletteItemPath& PathToAppend)
{
	if (PathToAppend.IsEmpty())
	{
		return;
	}

	if (IsEmpty())
	{
		*this = PathToAppend;
		return;
	}

	ParentItems.Add(Item);
	ParentItems.Append(PathToAppend.ParentItems);

	Item = PathToAppend.Item;
}

FString FMetaHumanPaletteItemPath::ToDebugString() const
{
	if (IsEmpty())
	{
		return TEXT("(empty path)");
	}

	if (ParentItems.Num() == 0)
	{
		return Item.ToDebugString();
	}

	const FString ParentPath = FString::JoinBy(ParentItems, TEXT("."), &FMetaHumanPaletteItemKey::ToDebugString);
	
	return ParentPath + TEXT(".") + Item.ToDebugString();
}

int32 FMetaHumanPaletteItemPath::Compare(const FMetaHumanPaletteItemPath& Other) const
{
	const int32 ThisNumEntries = GetNumPathEntries();
	const int32 OtherNumEntries = Other.GetNumPathEntries();

	if (ThisNumEntries > OtherNumEntries)
	{
		// Shorter paths are always "less" than longer paths
		return 1;
	}

	// We now know that ThisNumEntries <= OtherNumEntries, so it's safe to iterate up to 
	// ThisNumEntries on both.

	for (int32 Index = 0; Index < ThisNumEntries; Index++)
	{
		const FMetaHumanPaletteItemKey ThisEntry = GetPathEntry(Index);
		const FMetaHumanPaletteItemKey OtherEntry = Other.GetPathEntry(Index);

		if (ThisEntry < OtherEntry)
		{
			return -1;
		}

		if (ThisEntry != OtherEntry)
		{
			// ThisEntry > OtherEntry
			return 1;
		}

		// ThisEntry == OtherEntry
	}

	// The two paths are equal up to ThisNumEntries.
	//
	// If they're the same length, the paths are equal. Otherwise, the Other path must be longer
	// and is therefore "more" than this path.
	
	if (ThisNumEntries == OtherNumEntries)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

bool operator<(const FMetaHumanPaletteItemPath& A, const FMetaHumanPaletteItemPath& B)
{
	return A.Compare(B) < 0;
}
