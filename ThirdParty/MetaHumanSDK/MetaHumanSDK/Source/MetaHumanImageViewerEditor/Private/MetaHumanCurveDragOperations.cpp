// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCurveDragOperations.h"
#include "MetaHumanCurveDataController.h"

#define LOCTEXT_NAMESPACE "MetaHumanCurveDataOperations"

// Point Drag Operations

void FMetaHumanPointDragOperation::Initialize(TSharedPtr<class FMetaHumanCurveDataController> InCurveDataController, const TSet<int32>& InSelectedIds)
{
	CurveDataController = InCurveDataController;
	SelectedPointIds = InSelectedIds;
}

void FMetaHumanPointDragOperation::OnBeginDrag(const FVector2D& InGeometry, const FPointerEvent& InMouseEvent)
{
	Transaction = MakeUnique<FScopedTransaction>(LOCTEXT("MetaHumanPointDragTransaction", "Move Control Vertices"));

	CurveDataController->GetContourData()->Modify();
	PreviousFramePosition = InGeometry;
}

void FMetaHumanPointDragOperation::OnDrag(const FVector2D& InGeometry, const FPointerEvent& InMouseEvent)
{
	double MouseMoveStartTime = FPlatformTime::Seconds();
	const bool bUseImageUV = true;
	if ((MouseMoveStartTime - DragLastTime) >= DragElapsed * 2)
	{
		const FVector2D ImagePosition = OnGetPostionOnImageDelegate.Execute(InGeometry, bUseImageUV);
		if (SelectedPointIds.Num() > 1)
		{
			const FVector2D StartPosition = OnGetPostionOnImageDelegate.Execute(PreviousFramePosition, bUseImageUV);
			FVector2D Offset = StartPosition - ImagePosition;
			CurveDataController->OffsetSelectedPoints(SelectedPointIds, Offset);
		}
		else if (SelectedPointIds.Num() == 1)
		{
			CurveDataController->MoveSelectedPoint(ImagePosition, *SelectedPointIds.begin());
		}

		PreviousFramePosition = InGeometry;
		if (CurveDataController->TriggerContourUpdate().IsBound())
		{
			CurveDataController->TriggerContourUpdate().Broadcast();
		}
	}

	double Now = FPlatformTime::Seconds();
	DragLastTime = Now;
	DragElapsed = Now - MouseMoveStartTime;
}

void FMetaHumanPointDragOperation::OnEndDrag()
{
	CurveDataController->UpdateDensePointsAfterDragging(SelectedPointIds);
}

void FMetaHumanPointDragOperation::OnDragOperationPaint(const FGeometry& InAllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 InPaintOnLayerId)
{
}

// Marque Selection

void FMetaHumanMarqueSelectOperation::Initialize(TSharedPtr<class FMetaHumanCurveDataController> InCurveDataController)
{
	CurveDataController = InCurveDataController;
}

void FMetaHumanMarqueSelectOperation::OnBeginDrag(const FVector2D& InCurrentPosition, const FPointerEvent& InMouseEvent)
{
	RealInitialPosition = InCurrentPosition;

	Marquee = FSlateRect(
		FMath::Min(RealInitialPosition.X, InCurrentPosition.X),
		FMath::Min(RealInitialPosition.Y, InCurrentPosition.Y),
		FMath::Max(RealInitialPosition.X, InCurrentPosition.X),
		FMath::Max(RealInitialPosition.Y, InCurrentPosition.Y)
	);
}

void FMetaHumanMarqueSelectOperation::OnDrag(const FVector2D& InCurrentPosition, const FPointerEvent& InMouseEvent)
{
	Marquee = FSlateRect(
		FMath::Min(RealInitialPosition.X, InCurrentPosition.X),
		FMath::Min(RealInitialPosition.Y, InCurrentPosition.Y),
		FMath::Max(RealInitialPosition.X, InCurrentPosition.X),
		FMath::Max(RealInitialPosition.Y, InCurrentPosition.Y)
	);
}

void FMetaHumanMarqueSelectOperation::OnEndDrag()
{
	ResolveMarqueeSelectionDelegate.Execute(Marquee);
}

void FMetaHumanMarqueSelectOperation::OnDragOperationPaint(const FGeometry& InAllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 InPaintOnLayerId)
{
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		InPaintOnLayerId,
		InAllottedGeometry.ToPaintGeometry(Marquee.GetBottomRight() - Marquee.GetTopLeft(), FSlateLayoutTransform(Marquee.GetTopLeft())),
		FAppStyle::GetBrush(TEXT("MarqueeSelection"))
	);
}

#undef LOCTEXT_NAMESPACE