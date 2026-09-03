// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/MediaViewerLibraryItemDragDropOperation.h"

#include "DetailLayoutBuilder.h"
#include "Library/MediaViewerLibrary.h"
#include "Library/MediaViewerLibraryGroup.h"
#include "Library/MediaViewerLibraryItem.h"
#include "Math/Color.h"
#include "MediaViewerLibraryItemDragDropOperation.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "MediaViewerLibraryItemDragDropOperation"

namespace UE::MediaViewer::Private
{

FMediaViewerLibraryItemDragDropOperation::FMediaViewerLibraryItemDragDropOperation(const TSharedRef<FMediaViewerLibrary>& InLibrary,
	const IMediaViewerLibrary::FGroupItem& InGroupItem)
	: GroupItem(InGroupItem)
	, Decorator(SNew(SBorder))
{
	CreateDecorator(InLibrary->GetItem(InGroupItem.ItemId));
	Construct();
}

const IMediaViewerLibrary::FGroupItem& FMediaViewerLibraryItemDragDropOperation::GetGroupItem() const
{
	return GroupItem;
}

TSharedPtr<SWidget> FMediaViewerLibraryItemDragDropOperation::GetDefaultDecorator() const
{
	return Decorator;
}

FCursorReply FMediaViewerLibraryItemDragDropOperation::OnCursorQuery()
{
	return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
}

void FMediaViewerLibraryItemDragDropOperation::CreateDecorator(const TSharedPtr<FMediaViewerLibraryItem>& InItem)
{
	if (!InItem.IsValid())
	{
		return;
	}

	ThumbnailBrush = InItem->CreateThumbnail();

	const FLinearColor SaveColor = InItem->IsTransient()
		? FLinearColor(0.2, 0.2, 0.2)
		: FLinearColor::White;

	const FText SaveToolTip = InItem->IsTransient()
		? LOCTEXT("TransientToolTip", "Cannot be saved between sessions.")
		: LOCTEXT("NotTransientToolTip", "Can be saved between sessions.");

	Decorator->SetContent(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.f, 5.f, 5.f, 5.f)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(ThumbnailBrush.IsValid() ? ThumbnailBrush.Get() : nullptr)
			.DesiredSizeOverride(FVector2D(24.f))
		] 
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(3.f, 5.f, 5.f, 5.f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InItem->Name)
				.ToolTipText(InItem->ToolTip)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InItem->GetItemTypeDisplayName())
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
	);
}

} // UE::MediaViewer::Private

#undef LOCTEXT_NAMESPACE
