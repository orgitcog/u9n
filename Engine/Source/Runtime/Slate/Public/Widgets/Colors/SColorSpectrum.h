// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Framework/SlateDelegates.h"

class FPaintArgs;
class FSlateWindowElementList;
struct FSlateBrush;

/**
 * Implements the color spectrum widget.
 */
class SColorSpectrum
	: public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(SColorSpectrum)
		: _SelectedColor()
		, _OnMouseCaptureBegin()
		, _OnMouseCaptureEnd()
		, _OnValueChanged()
		, _CtrlMultiplier(0.1f)
	{ }
	
		/** The current color selected by the user */
		SLATE_ATTRIBUTE(FLinearColor, SelectedColor)
		
		/** Invoked when the mouse is pressed and a capture begins */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureBegin)

		/** Invoked when the mouse is released and a capture ends */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureEnd)

		/** Invoked when a new value is selected on the color spectrum */
		SLATE_EVENT(FOnLinearColorValueChanged, OnValueChanged)

		/** Multiplier to use when ctrl is held down */
		SLATE_ATTRIBUTE(float, CtrlMultiplier)

	SLATE_END_ARGS()
	
public:
	SLATE_API SColorSpectrum();
	SLATE_API virtual ~SColorSpectrum();

	/**
	 * Construct this widget.
	 *
	 * @param InArgs The declaration data for this widget.
	 */
	SLATE_API void Construct( const FArguments& InArgs );

public:

	// SWidget overrides

	SLATE_API virtual FVector2D ComputeDesiredSize(float) const override;
	SLATE_API virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	SLATE_API virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	SLATE_API virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	SLATE_API virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	SLATE_API virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	SLATE_API virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

protected:

	/**
	 * Calculates the position of the color selection indicator.
	 *
	 * @return The position relative to the widget.
	 */
	SLATE_API FVector2D CalcRelativeSelectedPosition( ) const;

	/**
	 * Performs actions according to mouse click / move
	 */
	SLATE_API void ProcessMouseAction(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

private:

	// The color spectrum image to show.
	const FSlateBrush* Image;
	
	// The current color selected by the user.
	TSlateAttribute<FLinearColor, EInvalidateWidgetReason::Paint> SelectedColor;

	// Mouse sensitivity multiplier to use when dragging the selector on the color spectrum, applied when the ctrl modifier key is pressed
	TAttribute<float> CtrlMultiplier;

	// The color selector image to show.
	const FSlateBrush* SelectorImage;

	// Whether the user is dragging the slider
	bool bDragging = false;

	// Cached mouse position to restore after dragging.
	FVector2f LastSpectrumPosition;

private:

	// Holds a delegate that is executed when the mouse is pressed and a capture begins.
	FSimpleDelegate OnMouseCaptureBegin;

	// Holds a delegate that is executed when the mouse is let up and a capture ends.
	FSimpleDelegate OnMouseCaptureEnd;

	// Holds a delegate that is executed when a new value is selected on the color spectrum.
	FOnLinearColorValueChanged OnValueChanged;
};
