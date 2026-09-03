// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Images/SImage.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnViewChanged, FBox2f);
DECLARE_MULTICAST_DELEGATE(FOnGeometryChanged);

class METAHUMANIMAGEVIEWER_API SMetaHumanImageViewer : public SImage
{
public:

	SLATE_BEGIN_ARGS(SMetaHumanImageViewer) {}

		/** Image resource */
		SLATE_ATTRIBUTE(const FSlateBrush*, Image)

		SLATE_ATTRIBUTE(TSharedPtr<class FUICommandList>, CommandList)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FReply HandleMouseButtonDown(const FGeometry& InGeometry, const FVector2f& InLocalMouse, const FKey& InEffectingButton);
	FReply HandleMouseButtonUp(const FGeometry& InGeometry, const FVector2f& InLocalMouse, const FKey& InEffectingButton);
	FReply HandleMouseMove(const FGeometry& InGeometry, const FVector2f& InLocalMouse);
	FReply HandleMouseWheel(const FGeometry& InGeometry, const FVector2f& InLocalMouse, float InWheelDelta);

	virtual FReply OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override;

	virtual int32 OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry,
		const FSlateRect& InWidgetClippingRect, FSlateWindowElementList& OutDrawElements,
		int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const override;

	FOnViewChanged OnViewChanged;

	void SetNonConstBrush(FSlateBrush* InBrush);
	virtual void ResetView();

	void SetDrawBlanking(bool bInDrawBlanking);

protected:

	TSharedPtr<class FUICommandList> CommandList;

	FBox2f UVOrig;
	FVector2f MouseOrig;
	bool bIsPanning = false;
	mutable FGeometry Geometry;

	FOnGeometryChanged OnGeometryChanged;
	FSlateBrush* NonConstBrush = nullptr;
	virtual void GeometryChanged();

	bool bDrawBlanking = true;
};
