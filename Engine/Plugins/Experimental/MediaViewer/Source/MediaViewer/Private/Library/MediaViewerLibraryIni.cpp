// Copyright Epic Games, Inc. All Rights Reserved.

#include "Library/MediaViewerLibraryIni.h"

#include "IMediaViewerModule.h"
#include "Library/MediaViewerLibrary.h"
#include "Library/MediaViewerLibraryGroup.h"
#include "Library/MediaViewerLibraryItem.h"
#include "Templates/SharedPointer.h"

UMediaViewerLibraryIni& UMediaViewerLibraryIni::Get()
{
	return *GetMutableDefault<UMediaViewerLibraryIni>();
}

void UMediaViewerLibraryIni::SaveLibrary(const TSharedRef<UE::MediaViewer::Private::FMediaViewerLibrary>& InLibrary)
{
	Groups.Empty();
	Groups.Reserve(InLibrary->Groups.Num());

	for (const TSharedRef<const FMediaViewerLibraryGroup>& Group : InLibrary->GetGroups())
	{
		if (Group->IsDynamic())
		{
			continue;
		}

		Groups.Add(*Group);
	}

	Items.Empty();
	Items.Reserve(InLibrary->Items.Num());

	for (const TPair<FGuid, TSharedRef<FMediaViewerLibraryItem>>& ItemPair : InLibrary->Items)
	{
		const TSharedRef<FMediaViewerLibraryItem>& Item = ItemPair.Value;

		if (Item->IsTransient())
		{
			continue;
		}

		Items.Emplace(Item->GetItemType(), *Item);
	}
}

void UMediaViewerLibraryIni::LoadLibrary(const TSharedRef<UE::MediaViewer::Private::FMediaViewerLibrary>& InLibrary) const
{
	UE::MediaViewer::IMediaViewerModule& Module = UE::MediaViewer::IMediaViewerModule::Get();

	for (const FMediaViewerLibraryItemData& ItemData : Items)
	{
		const FGuid ItemId = ItemData.Item.GetId();

		if (!InLibrary->Items.Contains(ItemId))
		{
			if (TSharedPtr<FMediaViewerLibraryItem> Item = Module.CreateLibraryItem(ItemData.ItemType, ItemData.Item))
			{
				InLibrary->Items.Add(ItemId, Item.ToSharedRef());
			}
		}
	}

	for (const FMediaViewerLibraryGroup& SavedGroup : Groups)
	{
		TSharedPtr<FMediaViewerLibraryGroup> Group = InLibrary->GetGroup(SavedGroup.GetId());

		if (!Group.IsValid())
		{
			Group = MakeShared<FMediaViewerLibraryGroup>(FMediaViewerLibraryGroup::FPrivateToken(), SavedGroup);
			InLibrary->AddGroup(Group.ToSharedRef());
		}

		TSet<FGuid> ExistingItems;
		ExistingItems.Append(Group->Items);

		for (const FGuid& ItemId : SavedGroup.Items)
		{
			if (!ExistingItems.Contains(ItemId))
			{
				Group->Items.Add(ItemId);
				ExistingItems.Add(ItemId);
			}
		}

		InLibrary->RemoveInvalidGroupItems(SavedGroup.GetId());
	}
}

bool UMediaViewerLibraryIni::HasGroup(const FGuid& InGroupId) const
{
	for (const FMediaViewerLibraryGroup& Group : Groups)
	{
		if (Group.GetId() == InGroupId)
		{
			return true;
		}
	}

	return false;
}

bool UMediaViewerLibraryIni::HasItem(const FGuid& InItemId) const
{
	for (const FMediaViewerLibraryItemData& ItemData : Items)
	{
		if (ItemData.Item.GetId() == InItemId)
		{
			return true;
		}
	}

	return false;
}

TConstArrayView<FMediaViewerState> UMediaViewerLibraryIni::GetSavedStates() const
{
	return SavedStates;
}

void UMediaViewerLibraryIni::SetSavedStates(const TArray<FMediaViewerState>& InStates)
{
	SavedStates = InStates;
}
