// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "ImageCore.h"

DECLARE_DELEGATE_TwoParams(FOnUVChanged, const FVector2f& InUV, bool bInIsDragging);

/**
 * Widget that displays a texture used as a color swatch. It allows selecting
 * a UV withing the swatch area
 */
class SUVColorSwatch : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUVColorSwatch) {}

		// The UV attribute used get the current UV value to display
		SLATE_ATTRIBUTE(FVector2f, UV)

		// The texture to use as the color swatch
		SLATE_ARGUMENT(class UTexture2D*, ColorPickerTexture)

		// Called when the the UV in the swatch changes
		SLATE_EVENT(FOnUVChanged, OnUVChanged);

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	//~Begin SCompoundWidget overrides
	virtual FReply OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual int32 OnPaint(const FPaintArgs& InArgs,
						  const FGeometry& InAllottedGeometry,
						  const FSlateRect& InWidgetClippingRect,
						  FSlateWindowElementList& OutDrawElements,
						  int32 InLayerId,
						  const FWidgetStyle& InWidgetStyle,
						  bool bInParentEnabled) const override;
	//~End SCompoundWidget overrides


private:

	// Slate Attributes
	TAttribute<FVector2f> UV;

	// Delegate to execute when the UV changes in the swatch area
	FOnUVChanged OnUVChangedDelegate;

	// Brush drawn to be used as a color picker
	FSlateBrush ColorPickerBrush;

	// The texture used as the color swatch
	TWeakObjectPtr<class UTexture2D> ColorPickerTexture;

	// True if the user is selecting a color using the picker
	bool bIsDragging = false;

	// The brush used to draw the cross hair showing which color is selected
	const FSlateBrush* CrosshairBrush = nullptr;
};

/**
 * Widget that displays a color block that when clicked creates
 * a pop-up window that allows the user to select a color from
 * a texture using UV coordinates
 */
class SUVColorPicker : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUVColorPicker)
		: _bUseSRGBInColorBlock(true)
	{}

		// The UV value currently displayed by the widget
		SLATE_ATTRIBUTE(FVector2f, UV)

		// The label of the color picker. Also used in the color swatch window title
		SLATE_ATTRIBUTE(FText, ColorPickerLabel)

		// Override for the U label in the picker window
		SLATE_ATTRIBUTE(FText, ULabelOverride)

		// Override for the V label in th picker window
		SLATE_ATTRIBUTE(FText, VLabelOverride)

		// Whether or not to use sRGB in the color block
		SLATE_ARGUMENT(bool, bUseSRGBInColorBlock)

		// The texture to use in the color swatch
		SLATE_ARGUMENT(class UTexture2D*, ColorPickerTexture)

		// Delegate called when the UV changes
		SLATE_EVENT(FOnUVChanged, OnUVChanged)

	SLATE_END_ARGS()

	virtual ~SUVColorPicker() override;

	void Construct(const FArguments& InArgs);
	
private:

	/** Called the color block is clicked */
	FReply OnUVColorBlockClicked(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent);

	/** Samples the texture image data using the current UV values */
	FLinearColor SampleTexture() const;

private:

	// The window to display the color picker
	TSharedPtr<class SWindow> Window;

	// Delegate called when the UV changes from the sliders
	FOnUVChanged OnUVChangedDelegate;

	// Label to use for the color block and the color picker window
	TAttribute<FText> ColorPickerLabel;

	// Overrides for the UV labels displayed in the color picker window
	TAttribute<FText> ULabelOverride;
	TAttribute<FText> VLabelOverride;

	// Get the current UV value
	TAttribute<FVector2f> UV;

	// Color picker texture to be used in the color swatch window
	TStrongObjectPtr<class UTexture2D> ColorPickerTexture;

	// Image data to be sampled by the color block
	FImage TextureImageData;
};