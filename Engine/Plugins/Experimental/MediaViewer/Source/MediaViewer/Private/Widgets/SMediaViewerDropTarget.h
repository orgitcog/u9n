// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "SMediaViewer.h"

namespace UE::MediaViewer::Private
{

class FMediaViewerLibraryItemDragDropOperation;
struct FMediaViewerDelegates;

class SMediaViewerDropTarget : public SCompoundWidget
{
	SLATE_DECLARE_WIDGET(SMediaViewerDropTarget, SCompoundWidget)

	SLATE_BEGIN_ARGS(SMediaViewerDropTarget)
		: _Position(EMediaImageViewerPosition::First)
		, _bComparisonView(false)
		, _bForceComparisonView(false)
		{}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(EMediaImageViewerPosition, Position)
		SLATE_ARGUMENT(bool, bComparisonView)
		SLATE_ARGUMENT(bool, bForceComparisonView)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FMediaViewerDelegates>& InDelegates);

	//~ Begin SWidget
	virtual void OnDragLeave(const FDragDropEvent& InDragDropEvent) override;
	//~ End SWidget

protected:
	static TArray<FAssetData> GetAssetsWithImageViewer(TConstArrayView<FAssetData> InAssets);

	TSharedPtr<FMediaViewerDelegates> Delegates;
	EMediaImageViewerPosition Position;
	bool bComparisonView;
	bool bForceComparisonView;

	EVisibility GetDragDescriptionVisibility() const;

	bool OnAllowDrop(TSharedPtr<FDragDropOperation> InDragDropOperation) const;

	bool OnIsRecognized(TSharedPtr<FDragDropOperation> InDragDropOperation) const;

	FReply OnDropped(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent);

	void HandleDroppedMediaViewerOp(const FMediaViewerLibraryItemDragDropOperation& InMediaViewerOp);

	void HandleDroppedAssets(const TArrayView<FAssetData> InDroppedAssets);
	
	void HandleDroppeFileOp(const FExternalDragOperation& InFileDragDropOp);
};

} // UE::MediaViewer::Private
