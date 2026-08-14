// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaViewerLibraryPrivate.h"

#include "ImageViewers/ColorImageViewer.h"
#include "LevelEditor.h"
#include "Library/LevelEditorViewportGroup.h"
#include "Library/MediaTextureGroup.h"
#include "Library/MediaViewerLibrary.h"
#include "Library/MediaViewerLibraryGroup.h"
#include "Library/MediaViewerLibraryIni.h"
#include "Library/MediaViewerLibraryItem.h"
#include "MediaViewer.h"
#include "Misc/Optional.h"
#include "Widgets/MediaViewerDelegates.h"
#include "Widgets/SMediaViewer.h"
#include "Widgets/SMediaViewerLibrary.h"

#define LOCTEXT_NAMESPACE "SMediaViewerLibraryPrivate"

namespace UE::MediaViewer::Private
{

SMediaViewerLibraryPrivate::SMediaViewerLibraryPrivate()
{
}

void SMediaViewerLibraryPrivate::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SMediaViewerLibraryPrivate::Construct(const FArguments& InArgs, const TSharedRef<FMediaViewerDelegates>& InDelegates)
{
	IMediaViewerLibraryWidget::FArgs Args;
	Args.GroupFilter = InArgs._GroupFilter;

	ChildSlot
	[
		SAssignNew(Library, SMediaViewerLibrary, Args, InDelegates)
	];

	TSharedRef<FMediaViewerLibrary> LibraryImpl = GetLibrary();

	LibraryImpl->AddGroup(MakeShared<FLevelEditorViewportGroup>(LibraryImpl));
	LibraryImpl->AddGroup(MakeShared<FMediaTextureGroup>(LibraryImpl));
}

TSharedRef<FMediaViewerLibrary> SMediaViewerLibraryPrivate::GetLibrary() const
{
	return Library->GetLibraryImpl();
}

void SMediaViewerLibraryPrivate::OnImageViewerOpened(const TSharedRef<FMediaImageViewer>& InImageViewer)
{
	TSharedRef<IMediaViewerLibrary> LibraryImpl = Library->GetLibrary();

	TSharedPtr<FMediaViewerLibraryGroup> HistoryGroup = LibraryImpl->GetGroup(LibraryImpl->GetHistoryGroupId());

	if (!HistoryGroup.IsValid())
	{
		return;
	}

	TSharedPtr<FMediaViewerLibraryItem> Item = LibraryImpl->GetItem(InImageViewer->GetInfo().Id);

	if (!Item.IsValid())
	{
		Item = InImageViewer->CreateLibraryItem();

		if (!Item.IsValid())
		{
			return;
		}

		TSharedPtr<FMediaViewerLibraryItem> ExistingItem = LibraryImpl->FindItemByValue(Item->GetItemType(), Item->GetStringValue());

		if (ExistingItem)
		{
			Item = ExistingItem;
			InImageViewer->UpdateId(ExistingItem->GetId());
		}
		else
		{
			LibraryImpl->AddItem(Item.ToSharedRef());
		}
	}

	HistoryGroup->RemoveItem(Item->GetId());

	HistoryGroup->AddItem(Item->GetId(), 0);

	while (HistoryGroup->GetItems().Num() > UE::MediaViewer::Private::MaxHistoryEntries)
	{
		HistoryGroup->RemoveItem(HistoryGroup->GetItems().Last());
	}

	Library->OnLibraryChanged(Library->GetLibrary(), IMediaViewerLibrary::EChangeType::ItemGroupChanged);
}

} // UE::MediaViewer::Private

#undef LOCTEXT_NAMESPACE
