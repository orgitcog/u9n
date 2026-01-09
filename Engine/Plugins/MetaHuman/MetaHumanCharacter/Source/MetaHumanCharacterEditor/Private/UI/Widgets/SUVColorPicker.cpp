// Copyright Epic Games, Inc. All Rights Reserved.

#include "SUVColorPicker.h"

#include "MetaHumanCharacterEditorStyle.h"

#include "Engine/Texture2D.h"
#include "UObject/ObjectKey.h"
#include "ImageUtils.h"
#include "Misc/Optional.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "SUVColorPicker"

void SUVColorSwatch::Construct(const FArguments& InArgs)
{
	check(InArgs._ColorPickerTexture);

	UV = InArgs._UV;
	OnUVChangedDelegate = InArgs._OnUVChanged;

	CrosshairBrush = FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("Skin.SkinTone.Crosshair"));

	ColorPickerTexture = InArgs._ColorPickerTexture;

	const FVector2f TextureSize(ColorPickerTexture->GetSizeX(), ColorPickerTexture->GetSizeY());

	ColorPickerBrush.SetResourceObject(ColorPickerTexture.Get());
	ColorPickerBrush.SetImageSize(TextureSize / 2.0f);

	ChildSlot
	[
		SNew(SImage)
		.Image(&ColorPickerBrush)
	];
}

FReply SUVColorSwatch::OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDragging = true;

		const FVector2f MousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		const FVector2f NewUV = MousePos / InGeometry.GetLocalSize();

		OnUVChangedDelegate.ExecuteIfBound(NewUV, bIsDragging);

		return FReply::Handled()
			.PreventThrottling()
			.CaptureMouse(SharedThis(this))
			.UseHighPrecisionMouseMovement(SharedThis(this))
			.SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}

	return FReply::Unhandled();
}

FReply SUVColorSwatch::OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
	{
		bIsDragging = false;

		const FVector2f MousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		FVector2f NewUV = MousePos / InGeometry.GetLocalSize();

		// Before showing the mouse position again, set the cursor position to be where the UV cursor is
		NewUV.X = FMath::Clamp(NewUV.X, 0.0f, 1.0f);
		NewUV.Y = FMath::Clamp(NewUV.Y, 0.0f, 1.0f);

		OnUVChangedDelegate.ExecuteIfBound(NewUV, bIsDragging);

		//  See SColorWheel::OnMouseButtonUp for how to position the cursor in a sensible location
		return FReply::Handled()
			.ReleaseMouseCapture()
			.SetMousePos(InGeometry.LocalToAbsolute(NewUV * InGeometry.GetLocalSize()).IntPoint());
	}

	return FReply::Unhandled();
}

void SUVColorSwatch::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	// This can happen if the character has high resolution textures, which will bring a modal message dialog asking
	// the user if they want to proceed. In this case OnMouseButtonUp will never be called but OnMouseLeave will,
	// so set bIsSelecting to false
	bIsDragging = false;
}

FReply SUVColorSwatch::OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!HasMouseCapture())
	{
		return FReply::Unhandled();
	}

	if (!bIsDragging)
	{
		bIsDragging = true;
	}

	const FVector2f MousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2f NewUV = MousePos / InGeometry.GetLocalSize();
	OnUVChangedDelegate.ExecuteIfBound(NewUV, bIsDragging);

	return FReply::Handled().PreventThrottling();
}

FCursorReply SUVColorSwatch::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return bIsDragging ?
		FCursorReply::Cursor(EMouseCursor::None) :
		FCursorReply::Cursor(EMouseCursor::Default);
}

int32 SUVColorSwatch::OnPaint(const FPaintArgs& InArgs,
							  const FGeometry& InAllottedGeometry,
							  const FSlateRect& InWidgetClippingRect,
							  FSlateWindowElementList& OutDrawElements,
							  int32 InLayerId,
							  const FWidgetStyle& InWidgetStyle,
							  bool bInParentEnabled) const
{
	int32 LayerId = SCompoundWidget::OnPaint(InArgs,
											 InAllottedGeometry,
											 InWidgetClippingRect,
											 OutDrawElements,
											 InLayerId,
											 InWidgetStyle,
											 bInParentEnabled);

	LayerId++;

	const FVector2f LocalSize = InAllottedGeometry.GetLocalSize();
	const FVector2f UVPixelPos = FMath::Lerp(FVector2f::ZeroVector, LocalSize, UV.Get());
	const FVector2f CrosshairBrushSize = CrosshairBrush->GetImageSize();
	const FVector2f CrosshairPos = UVPixelPos - (CrosshairBrushSize / 2.0f);
	const FGeometry CrosshairGeometry = InAllottedGeometry.MakeChild(CrosshairBrushSize, FSlateLayoutTransform{ CrosshairPos });

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, CrosshairGeometry.ToPaintGeometry(), CrosshairBrush);

	return LayerId;
}

void SUVColorPicker::Construct(const FArguments& InArgs)
{
	UV = InArgs._UV;
	ColorPickerTexture = TStrongObjectPtr<UTexture2D>{ InArgs._ColorPickerTexture };
	ColorPickerLabel = InArgs._ColorPickerLabel;
	OnUVChangedDelegate = InArgs._OnUVChanged;

	check(ColorPickerTexture);

	const bool bImageRead = FImageUtils::GetTexture2DSourceImage(ColorPickerTexture.Get(), TextureImageData);
	check(bImageRead);

	ChildSlot
	[
		SNew(SColorBlock)			
		.Color(this, &SUVColorPicker::SampleTexture)
		.UseSRGB(InArgs._bUseSRGBInColorBlock)
		.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
		.AlphaBackgroundBrush(FAppStyle::Get().GetBrush("ColorPicker.RoundedAlphaBackground"))
		.ShowBackgroundForAlpha(true)
		.CornerRadius(FVector4(2.f, 2.f, 2.f, 2.f))
		.OnMouseButtonDown(this, &SUVColorPicker::OnUVColorBlockClicked)
	];
}

FReply SUVColorPicker::OnUVColorBlockClicked(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent)
{
	if (Window.IsValid())
	{
		Window->RequestDestroyWindow();
	}

	// Determine the position of the window so that it will spawn near the mouse, but not go off the screen.
	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
	const FSlateRect Anchor(CursorPos.X, CursorPos.Y, CursorPos.X, CursorPos.Y);

	const FVector2D DefaultWindowSize{ 450.0, 250.0 };
	const bool bAutoAdjustForDPIScale = true;
	const FVector2D ProposedPlacement = FVector2D::ZeroVector;
	const FVector2D AdjustedSummonLocation = FSlateApplication::Get().CalculatePopupWindowPosition(Anchor,
																								   DefaultWindowSize,
																								   bAutoAdjustForDPIScale,
																								   ProposedPlacement,
																								   Orient_Horizontal);

	SAssignNew(Window, SWindow)
		.AutoCenter(EAutoCenter::None)
		.ScreenPosition(AdjustedSummonLocation)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize)
		.ClientSize(DefaultWindowSize)
		.Title(ColorPickerLabel)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(FMargin{ 16.0, 16.0 })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SUVColorSwatch)
					.UV(UV)
					.ColorPickerTexture(ColorPickerTexture.Get())
					.OnUVChanged(OnUVChangedDelegate)
				]

				// U Slider and Label

				+ SVerticalBox::Slot()
				.Padding(1.0f, 5.0f)
				[
					SNew(SHorizontalBox)

					// U Label Section
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.FillWidth(0.2f)
					.Padding(10.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text_Lambda([this]
						{
							if (ULabelOverride.IsBound() && !ULabelOverride.Get().IsEmptyOrWhitespace())
							{
								return ULabelOverride.Get();
							}
							else
							{
								return LOCTEXT("DefaultULabel", "U");
							}
						})
					]

					// U Slider Section
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					.FillWidth(0.8f)
					.Padding(4.0f, 2.0f, 80.0f, 2.0f)
					[
						SNew(SNumericEntryBox<float>)
						.AllowSpin(true)
						.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.SpinBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
						.MinValue(0.0f)
						.MaxValue(1.0f)
						.MinSliderValue(0.0f)
						.MaxSliderValue(1.0f)
						.PreventThrottling(true)
						.MaxFractionalDigits(2)
						.LinearDeltaSensitivity(1.0)
						.Value_Lambda([this]
						{
							return UV.Get().X;
						})
						.OnValueChanged_Lambda([this](float NewUValue)
						{
							const bool bIsInteractive = true;
							OnUVChangedDelegate.ExecuteIfBound(FVector2f{ NewUValue, UV.Get().Y }, bIsInteractive);
						})
						.OnValueCommitted_Lambda([this](float NewUValue, ETextCommit::Type InType)
						{
							const bool bIsInteractive = false;
							OnUVChangedDelegate.ExecuteIfBound(FVector2f{ NewUValue, UV.Get().Y }, bIsInteractive);
						})
					]
				]

				// V Label and Slider

				+ SVerticalBox::Slot()
				.Padding(1.0f, 5.0f)
				[
					SNew(SHorizontalBox)

					// V Label Section
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.FillWidth(0.2f)
					.Padding(10.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text_Lambda([this]
						{
							if (VLabelOverride.IsBound() && !VLabelOverride.Get().IsEmptyOrWhitespace())
							{
								return VLabelOverride.Get();
							}
							else
							{
								return LOCTEXT("DefaultVLabel", "V");
							}
						})
					]

					// V Slider Section
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					.FillWidth(0.8f)
					.Padding(4.0f, 2.0f, 80.0f, 2.0f)
					[
						SNew(SNumericEntryBox<float>)
						.AllowSpin(true)
						.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.SpinBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
						.MinValue(0.0f)
						.MaxValue(1.0f)
						.MinSliderValue(0.0f)
						.MaxSliderValue(1.0f)
						.PreventThrottling(true)
						.MaxFractionalDigits(2)
						.LinearDeltaSensitivity(1.0)
						.Value_Lambda([this]
						{
							return UV.Get().Y;
						})
						.OnValueChanged_Lambda([this](float NewVValue)
						{
							const bool bIsInteractive = true;
							OnUVChangedDelegate.ExecuteIfBound(FVector2f{ UV.Get().X, NewVValue }, bIsInteractive);
						})
						.OnValueCommitted_Lambda([this](float NewVValue, ETextCommit::Type InType)
						{
							const bool bIsInteractive = false;
							OnUVChangedDelegate.ExecuteIfBound(FVector2f{ UV.Get().X, NewVValue }, bIsInteractive);
						})
					]
				]
			]
		];

	// Find the window of the parent widget
	FWidgetPath WidgetPath;
	FSlateApplication::Get().GeneratePathToWidgetChecked(SharedThis(this), WidgetPath);
	Window = FSlateApplication::Get().AddWindowAsNativeChild(Window.ToSharedRef(), WidgetPath.GetWindow());

	return FReply::Handled();
}

FLinearColor SUVColorPicker::SampleTexture() const
{
	const FVector2f UVValue = UV.Get();

	const FInt64Vector2 PixelCoords =
	{
		FMath::RoundToInt((TextureImageData.GetWidth() - 1) * UVValue.X),
		FMath::RoundToInt((TextureImageData.GetHeight() - 1) * UVValue.Y)
	};
	return TextureImageData.AsBGRA8()[PixelCoords.X + PixelCoords.Y * TextureImageData.GetWidth()];
}

SUVColorPicker::~SUVColorPicker()
{
	if (Window.IsValid())
	{
		Window->RequestDestroyWindow();
	}	
}

#undef LOCTEXT_NAMESPACE