// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/DelayedDrag.h"
#include "Templates/UniquePtr.h"
#include "ScopedTransaction.h"

DECLARE_DELEGATE_RetVal_TwoParams(FVector2D, FOnGetPositionOnImage, const FVector2D&, bool bUseImageUV);
DECLARE_DELEGATE_OneParam(FOnResolveMarqueeSelection, const FSlateRect& InSelection)

class IMetaHumanEditorDragOperation
{
public:
	virtual ~IMetaHumanEditorDragOperation() {};
	virtual void OnBeginDrag(const FVector2D& InGeometry, const FPointerEvent& InMouseEvent) = 0;
	virtual void OnDrag(const FVector2D& InGeometry, const FPointerEvent& InMouseEvent) = 0;
	virtual void OnEndDrag() = 0;
	virtual void OnDragOperationPaint(const FGeometry& InAllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 InPaintOnLayerId) = 0;
};

class FMetaHumanPointDragOperation : public IMetaHumanEditorDragOperation
{
public:

	virtual void OnBeginDrag(const FVector2D& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void OnDrag(const FVector2D& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void OnEndDrag() override;
	virtual void OnDragOperationPaint(const FGeometry& InAllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 InPaintOnLayerId) override;

	void Initialize(TSharedPtr<class FMetaHumanCurveDataController> InCurveDataController, const TSet<int32>& InSelectedIds);

	FOnGetPositionOnImage OnGetPostionOnImageDelegate;

private:

	TSet<int32> SelectedPointIds;
	FVector2D PreviousFramePosition;
	double DragElapsed = 0.0;
	double DragLastTime = 0.0;

	TSharedPtr<class FMetaHumanCurveDataController> CurveDataController;
	TUniquePtr<class FScopedTransaction> Transaction;
};

class FMetaHumanMarqueSelectOperation : public IMetaHumanEditorDragOperation
{
public:

	virtual void OnBeginDrag(const FVector2D& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void OnDrag(const FVector2D& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void OnEndDrag() override;
	virtual void OnDragOperationPaint(const FGeometry& InAllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 InPaintOnLayerId) override;

	void Initialize(TSharedPtr<class FMetaHumanCurveDataController> InCurveDataController);

	FOnResolveMarqueeSelection ResolveMarqueeSelectionDelegate;

private:

	/** The current marquee rectangle */
	FSlateRect Marquee;

	/** Real Initial Position do to the delayed drag*/
	FVector2D RealInitialPosition;

	TSharedPtr<class FMetaHumanCurveDataController> CurveDataController;
};

struct FMetaHumanCurveEditorDelayedDrag : FDelayedDrag
{
	//The drag implementation to use once the drag has started
	TUniquePtr<IMetaHumanEditorDragOperation> DragImpl;

	//Start a delayed drag operation at the specified position and effective key
	FMetaHumanCurveEditorDelayedDrag(FVector2D InInitialPosition, FKey InEffectiveKey)
		: FDelayedDrag(InInitialPosition, InEffectiveKey)
	{
		SetTriggerScaleFactor(0.01f);
	}
};