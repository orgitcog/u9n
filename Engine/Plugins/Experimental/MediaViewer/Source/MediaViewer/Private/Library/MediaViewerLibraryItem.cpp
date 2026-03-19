// Copyright Epic Games, Inc. All Rights Reserved.

#include "Library/MediaViewerLibraryItem.h"

#define LOCTEXT_NAMESPACE "MediaViewerLibraryItem"

FMediaViewerLibraryItem::FMediaViewerLibraryItem()
	: FMediaViewerLibraryItem(FText::GetEmpty(), FText::GetEmpty(), /* Transient */ false, TEXT(""))
{
}

FMediaViewerLibraryItem::FMediaViewerLibraryItem(const FText& InName, const FText& InToolTip,
	bool bInTransient, const FString& InStringValue)
	: FMediaViewerLibraryItem(FGuid::NewGuid(), InName, InToolTip, bInTransient, InStringValue)
{
}

FMediaViewerLibraryItem::FMediaViewerLibraryItem(const FGuid& InId, const FText& InName, const FText& InToolTip, 
	bool bInTransient, const FString& InStringValue)
	: FMediaViewerLibraryEntry(InId, InName, InToolTip)
	, bTransient(bInTransient)
	, StringValue(InStringValue)
{
}

FName FMediaViewerLibraryItem::GetItemType() const
{
	return NAME_None;
}

FText FMediaViewerLibraryItem::GetItemTypeDisplayName() const
{
	return LOCTEXT("Error", "Error");
}

bool FMediaViewerLibraryItem::IsTransient() const
{
	return bTransient;
}

const FString& FMediaViewerLibraryItem::GetStringValue() const
{
	return StringValue;
}

#undef LOCTEXT_NAMESPACE
