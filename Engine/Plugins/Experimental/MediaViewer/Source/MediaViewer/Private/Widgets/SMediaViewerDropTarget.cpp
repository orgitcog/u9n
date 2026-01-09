// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaViewerDropTarget.h"

#include "AssetSelection.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "FileMediaSource.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageViewers/MediaSourceImageViewer.h"
#include "IMediaViewerModule.h"
#include "Input/DragAndDrop.h"
#include "Library/MediaViewerLibrary.h"
#include "Library/MediaViewerLibraryItem.h"
#include "MediaPlayer.h"
#include "SDropTarget.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/MediaViewerDelegates.h"
#include "Widgets/MediaViewerLibraryItemDragDropOperation.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "SMediaViewerDropTarget"

namespace UE::MediaViewer::Private
{

void SMediaViewerDropTarget::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SMediaViewerDropTarget::Construct(const FArguments& InArgs, const TSharedRef<FMediaViewerDelegates>& InDelegates)
{
	Delegates = InDelegates;
	Position = InArgs._Position;
	bComparisonView = InArgs._bComparisonView;
	bForceComparisonView = InArgs._bForceComparisonView;

	FText UpperMessage;

	if (bComparisonView || (Position == EMediaImageViewerPosition::First && !bForceComparisonView))
	{
		UpperMessage = LOCTEXT("ReplaceImage", "Replace Image");
	}
	else
	{
		UpperMessage = LOCTEXT("CompareImage", "Compare Image");
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SDropTarget)
			.OnAllowDrop(this, &SMediaViewerDropTarget::OnAllowDrop)
			.OnIsRecognized(this, &SMediaViewerDropTarget::OnIsRecognized)
			.OnDropped(this, &SMediaViewerDropTarget::OnDropped)
		]
		+ SOverlay::Slot()
		[
			InArgs._Content.Widget
		]
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(0.f, 0.f, 0.f, 5.f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::Foreground.GetSpecifiedColor())
				.ShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor())
				.ShadowOffset(FVector2D(1.f, 1.f))
				.Visibility(this, &SMediaViewerDropTarget::GetDragDescriptionVisibility)
				.Text(UpperMessage)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(5.f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::Foreground.GetSpecifiedColor())
				.ShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor())
				.ShadowOffset(FVector2D(1.f, 1.f))
				.Visibility(this, &SMediaViewerDropTarget::GetDragDescriptionVisibility)
				.Text(LOCTEXT("DropTargetMessage", "Drop supported asset or library item here."))
				.AutoWrapText(true)
			]
		]
	];
}

void SMediaViewerDropTarget::OnDragLeave(const FDragDropEvent& InDragDropEvent)
{
	TSharedPtr<FDragDropOperation> Operation = InDragDropEvent.GetOperation();

	if (Operation.IsValid() && Operation->IsOfType<FDecoratedDragDropOp>())
	{
		TSharedPtr<FDecoratedDragDropOp> DragDropOp = StaticCastSharedPtr<FDecoratedDragDropOp>(Operation);
		DragDropOp->ResetToDefaultToolTip();
	}
}

TArray<FAssetData> SMediaViewerDropTarget::GetAssetsWithImageViewer(TConstArrayView<FAssetData> InAssets)
{
	IMediaViewerModule& MediaViewerModule = IMediaViewerModule::Get();

	TArray<FAssetData> ValidAssets;

	for (const FAssetData& AssetData : InAssets)
	{
		if (MediaViewerModule.HasFactoryFor(AssetData))
		{
			ValidAssets.Add(AssetData);
		}
	}

	return ValidAssets;
}

EVisibility SMediaViewerDropTarget::GetDragDescriptionVisibility() const
{
	return FSlateApplication::Get().IsDragDropping()
		? EVisibility::HitTestInvisible
		: EVisibility::Collapsed;
}

FReply SMediaViewerDropTarget::OnDropped(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent)
{
	TSharedPtr<FDragDropOperation> DragDropOp = InDragDropEvent.GetOperation();

	if (!DragDropOp.IsValid())
	{
		return FReply::Handled();
	}

	if (TSharedPtr<FMediaViewerLibraryItemDragDropOperation> MediaViewerOp = InDragDropEvent.GetOperationAs<FMediaViewerLibraryItemDragDropOperation>())
	{
		HandleDroppedMediaViewerOp(*MediaViewerOp);
		return FReply::Handled();
	}

	if (TSharedPtr<FExternalDragOperation> FileOp = InDragDropEvent.GetOperationAs<FExternalDragOperation>())
	{
		HandleDroppeFileOp(*FileOp);
		return FReply::Handled();
	}

	TArray<FAssetData> DroppedAssets = AssetUtil::ExtractAssetDataFromDrag(DragDropOp);

	if (!DroppedAssets.IsEmpty())
	{
		HandleDroppedAssets(DroppedAssets);
	}

	return FReply::Handled();
}

void SMediaViewerDropTarget::HandleDroppedMediaViewerOp(const FMediaViewerLibraryItemDragDropOperation& InMediaViewerOp)
{
	TSharedRef<FMediaViewerLibrary> Library = Delegates->GetLibrary.Execute();

	if (TSharedPtr<FMediaViewerLibraryItem> LibraryItem = Library->GetItem(InMediaViewerOp.GetGroupItem().ItemId))
	{
		if (TSharedPtr<FMediaImageViewer> ImageViewer = LibraryItem->CreateImageViewer())
		{
			// Swap images and then replace first image.
			if (bForceComparisonView && Position == EMediaImageViewerPosition::First)
			{
				Delegates->SwapAB.Execute();
				Delegates->SetABView.Execute();
				Delegates->SetABOrientation.Execute(Orient_Horizontal);
			}

			Delegates->SetImageViewer.Execute(Position, ImageViewer.ToSharedRef());
		}
	}
}

void SMediaViewerDropTarget::HandleDroppedAssets(const TArrayView<FAssetData> InDroppedAssets)
{
	IMediaViewerModule& MediaViewerModule = IMediaViewerModule::Get();

	const TArray<FAssetData> ValidAssets = GetAssetsWithImageViewer(InDroppedAssets);
	TArray<TSharedRef<FMediaImageViewer>> ImageViewers;

	for (const FAssetData& AssetData : ValidAssets)
	{
		if (TSharedPtr<FMediaViewerLibraryItem> LibraryItem = MediaViewerModule.CreateLibraryItem(AssetData))
		{
			if (TSharedPtr<FMediaImageViewer> ImageViewer = LibraryItem->CreateImageViewer())
			{
				ImageViewers.Add(ImageViewer.ToSharedRef());

				if (ImageViewers.Num() == static_cast<int32>(EMediaImageViewerPosition::COUNT))
				{
					break;
				}
			}
		}
	}

	if (ImageViewers.Num() > 1)
	{
		for (int32 Index = 0; Index < static_cast<int32>(EMediaImageViewerPosition::COUNT); ++Index)
		{
			Delegates->SetImageViewer.Execute(static_cast<EMediaImageViewerPosition>(Index), ImageViewers[Index]);
		}
	}
	else if (ImageViewers.Num() == 1)
	{
		// Swap images and then replace first image.
		if (bForceComparisonView && Position == EMediaImageViewerPosition::First)
		{
			Delegates->SwapAB.Execute();
			Delegates->SetABView.Execute();
			Delegates->SetABOrientation.Execute(Orient_Horizontal);
		}

		Delegates->SetImageViewer.Execute(Position, ImageViewers[0]);
	}
}

void SMediaViewerDropTarget::HandleDroppeFileOp(const FExternalDragOperation& InFileDragDropOp)
{
	UMediaPlayer* TestMediaPlayer = NewObject<UMediaPlayer>(GetTransientPackage());
	TArray<UFileMediaSource*, TInlineAllocator<2>> Sources;

	for (const FString& FileName : InFileDragDropOp.GetFiles())
	{
		if (!TestMediaPlayer->OpenFile(FileName))
		{
			continue;
		}

		UFileMediaSource* FileMediaSource = NewObject<UFileMediaSource>(GetTransientPackage());
		FileMediaSource->SetFilePath(FileName);

		Sources.Add(FileMediaSource);

		if (Sources.Num() == 2)
		{
			break;
		}
	}

	TestMediaPlayer->Close();

	if (Sources.Num() > 1)
	{
		for (int32 Index = 0; Index < Sources.Num(); ++Index)
		{
			Delegates->SetImageViewer.Execute(
				static_cast<EMediaImageViewerPosition>(Index),
				MakeShared<FMediaSourceImageViewer>(Sources[Index], FText::FromString(Sources[Index]->GetFilePath()))
			);
		}
	}
	else if (Sources.Num() == 1)
	{
		// Swap images and then replace first image.
		if (bForceComparisonView && Position == EMediaImageViewerPosition::First)
		{
			Delegates->SwapAB.Execute();
			Delegates->SetABView.Execute();
			Delegates->SetABOrientation.Execute(Orient_Horizontal);
		}

		Delegates->SetImageViewer.Execute(
			EMediaImageViewerPosition::First,
			MakeShared<FMediaSourceImageViewer>(Sources[0], FText::FromString(Sources[0]->GetFilePath()))
		);
	}
}

bool SMediaViewerDropTarget::OnAllowDrop(TSharedPtr<FDragDropOperation> InDragDropOperation) const
{
	if (!InDragDropOperation.IsValid())
	{
		return false;
	}

	if (InDragDropOperation->IsOfType<FMediaViewerLibraryItemDragDropOperation>())
	{
		TSharedPtr<FMediaViewerLibraryItemDragDropOperation> LibraryItemDragDropOp = StaticCastSharedPtr<FMediaViewerLibraryItemDragDropOperation>(InDragDropOperation);

		if (!Delegates->GetLibrary.Execute()->GetItem(LibraryItemDragDropOp->GetGroupItem().ItemId).IsValid())
		{
			if (IsHovered())
			{
				LibraryItemDragDropOp->SetToolTip(LOCTEXT("InvalidItem", "Invalid Library Item"), FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
			}

			return false;
		}

		return true;
	}

	if (InDragDropOperation->IsOfType<FExternalDragOperation>())
	{
		TSharedPtr<FExternalDragOperation> ExternalDragDropOp = StaticCastSharedPtr<FExternalDragOperation>(InDragDropOperation);
		return ExternalDragDropOp->HasFiles();
	}

	const TArray<FAssetData> DroppedAssets = AssetUtil::ExtractAssetDataFromDrag(InDragDropOperation);
	const TArray<FAssetData> ValidAssets = GetAssetsWithImageViewer(DroppedAssets);

	if (ValidAssets.IsEmpty())
	{
		if (InDragDropOperation->IsOfType<FDecoratedDragDropOp>())
		{
			TSharedPtr<FDecoratedDragDropOp> DecoratedDragDropOp = StaticCastSharedPtr<FDecoratedDragDropOp>(InDragDropOperation);
			DecoratedDragDropOp->SetToolTip(LOCTEXT("NotSupported", "Not Supported"), FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
		}

		return false;
	}

	return true;
}

bool SMediaViewerDropTarget::OnIsRecognized(TSharedPtr<FDragDropOperation> InDragDropOperation) const
{
	if (!InDragDropOperation.IsValid())
	{
		return false;
	}

	if (InDragDropOperation->IsOfType<FMediaViewerLibraryItemDragDropOperation>())
	{
		return true;
	}

	if (InDragDropOperation->IsOfType<FExternalDragOperation>())
	{
		return true;
	}

	const TArray<FAssetData> DroppedAssets = AssetUtil::ExtractAssetDataFromDrag(InDragDropOperation);

	return !DroppedAssets.IsEmpty();
}

}

#undef LOCTEXT_NAMESPACE
