// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanImageViewer.h"
#include "Brushes/SlateColorBrush.h"

#include "Framework/Commands/UICommandList.h"

void SMetaHumanImageViewer::Construct(const FArguments& InArgs)
{
	SImage::Construct(SImage::FArguments()
					  .Image(InArgs._Image));

	if (InArgs._CommandList.IsSet())
	{
		CommandList = InArgs._CommandList.Get();
	}
	else
	{
		CommandList = MakeShared<FUICommandList>();
	}
}

FReply SMetaHumanImageViewer::HandleMouseButtonDown(const FGeometry& InGeometry, const FVector2f& InLocalMouse, const FKey& InEffectingButton)
{
	FReply Reply = FReply::Unhandled();

	if (!bIsPanning && InEffectingButton == EKeys::RightMouseButton)
	{
		MouseOrig.X = InLocalMouse.X / InGeometry.GetLocalSize().X;
		MouseOrig.Y = InLocalMouse.Y / InGeometry.GetLocalSize().Y;

		UVOrig = GetImageAttribute().Get()->GetUVRegion();

		bIsPanning = true;

		Reply = FReply::Handled();
	}

	if (Reply.IsEventHandled())
	{
		Reply.CaptureMouse(SharedThis(this));
	}

	return Reply;
}

FReply SMetaHumanImageViewer::HandleMouseButtonUp(const FGeometry& InGeometry, const FVector2f& InLocalMouse, const FKey& InEffectingButton)
{
	FReply Reply = FReply::Unhandled();

	if (bIsPanning && InEffectingButton == EKeys::RightMouseButton)
	{
		bIsPanning = false;

		Reply = FReply::Handled();
	}

	if (Reply.IsEventHandled())
	{
		Reply.ReleaseMouseCapture();
	}

	return Reply;
}

FReply SMetaHumanImageViewer::HandleMouseMove(const FGeometry& InGeometry, const FVector2f& InLocalMouse)
{
	FReply Reply = FReply::Unhandled();

	if (bIsPanning)
	{
		FVector2f Mouse;
		Mouse.X = InLocalMouse.X / InGeometry.GetLocalSize().X;
		Mouse.Y = InLocalMouse.Y / InGeometry.GetLocalSize().Y;

		FVector2f MouseDelta = MouseOrig - Mouse;

		FBox2f UV = UVOrig;
		UV = UV.ShiftBy(FVector2f(MouseDelta.X * (UV.Max.X - UV.Min.X), MouseDelta.Y * (UV.Max.Y - UV.Min.Y)));

		OnViewChanged.Broadcast(UV);

		Reply = FReply::Handled();
	}

	return Reply;
}

FReply SMetaHumanImageViewer::HandleMouseWheel(const FGeometry& InGeometry, const FVector2f& InLocalMouse, float InWheelDelta)
{
	float X = InLocalMouse.X / InGeometry.GetLocalSize().X;
	float Y = InLocalMouse.Y / InGeometry.GetLocalSize().Y;

	FBox2f UV = GetImageAttribute().Get()->GetUVRegion();

	X = UV.Min.X + X * (UV.Max.X - UV.Min.X);
	Y = UV.Min.Y + Y * (UV.Max.Y - UV.Min.Y);

	float Delta = InWheelDelta < 0 ? 1.1 : 1.0 / 1.1;

	UV.Min.X = X - (X - UV.Min.X) * Delta;
	UV.Max.X = X + (UV.Max.X - X) * Delta;
	UV.Min.Y = Y - (Y - UV.Min.Y) * Delta;
	UV.Max.Y = Y + (UV.Max.Y - Y) * Delta;

	OnViewChanged.Broadcast(UV);

	return FReply::Handled();
}

FReply SMetaHumanImageViewer::OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return HandleMouseButtonDown(InGeometry, InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()), InMouseEvent.GetEffectingButton());
}

FReply SMetaHumanImageViewer::OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return HandleMouseButtonUp(InGeometry, InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()), InMouseEvent.GetEffectingButton());
}

FReply SMetaHumanImageViewer::OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return HandleMouseMove(InGeometry, InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
}

FReply SMetaHumanImageViewer::OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return HandleMouseWheel(InGeometry, InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()), InMouseEvent.GetWheelDelta());
}

FReply SMetaHumanImageViewer::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	else
	{
		return SImage::OnKeyDown(InGeometry, InKeyEvent);
	}
}

bool SMetaHumanImageViewer::SupportsKeyboardFocus() const
{
	return true;
}

int32 SMetaHumanImageViewer::OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry,
	const FSlateRect& InWidgetClippingRect, FSlateWindowElementList& OutDrawElements,
	int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const
{
	if (InAllottedGeometry != Geometry)
	{
		Geometry = InAllottedGeometry;
		OnGeometryChanged.Broadcast();
	}

	FSlateColorBrush Brush = FSlateColorBrush(FLinearColor::White);
	FLinearColor Colour = FLinearColor(0, 0, 0);
	int32 LayerId = InLayerId;

	if (!GetImageAttribute().Get()->GetResourceObject()) // fill window with black if nothing to display
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, InAllottedGeometry.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, Colour);
		LayerId++;

		return LayerId;
	}

	LayerId = SImage::OnPaint(InArgs, InAllottedGeometry, InWidgetClippingRect, OutDrawElements, LayerId, InWidgetStyle, InParentEnabled);
	LayerId++;

	if (bDrawBlanking)
	{
		FBox2f UV = GetImageAttribute().Get()->GetUVRegion();
		FVector2f Size = InAllottedGeometry.GetLocalSize();

		if (UV.Min.X < 0)
		{
			float Factor = FMath::Min(-UV.Min.X / (UV.Max.X - UV.Min.X), 1);
			FGeometry Box = InAllottedGeometry.MakeChild(FVector2f(Factor * Size.X, Size.Y), FSlateLayoutTransform(1.f, FVector2f(0, 0)));

			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Box.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, Colour);
			LayerId++;
		}

		if (UV.Min.Y < 0)
		{
			float Factor = FMath::Min(-UV.Min.Y / (UV.Max.Y - UV.Min.Y), 1);
			FGeometry Box = InAllottedGeometry.MakeChild(FVector2f(Size.X, Factor * Size.Y), FSlateLayoutTransform(1.f, FVector2f(0, 0)));

			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Box.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, Colour);
			LayerId++;
		}

		if (UV.Max.X > 1)
		{
			float Factor = FMath::Max((-UV.Min.X + 1) / (UV.Max.X - UV.Min.X), 0);
			FGeometry Box = InAllottedGeometry.MakeChild(FVector2f(Size.X - Factor * Size.X, Size.Y), FSlateLayoutTransform(1.f, FVector2f(Factor * Size.X, 0)));

			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Box.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, Colour);
			LayerId++;
		}

		if (UV.Max.Y > 1)
		{
			float Factor = FMath::Max((-UV.Min.Y + 1) / (UV.Max.Y - UV.Min.Y), 0);
			FGeometry box = InAllottedGeometry.MakeChild(FVector2f(Size.X, Size.Y - Factor * Size.Y), FSlateLayoutTransform(1.f, FVector2f(0, Factor * Size.Y)));

			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, box.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, Colour);
			LayerId++;
		}
	}

	return LayerId;
}

void SMetaHumanImageViewer::SetNonConstBrush(FSlateBrush* InBrush)
{ 
	NonConstBrush = InBrush;

	OnGeometryChanged.AddRaw(this, &SMetaHumanImageViewer::GeometryChanged);
}

void SMetaHumanImageViewer::ResetView()
{
	if (NonConstBrush)
	{
		GeometryChanged();
	}
}

void SMetaHumanImageViewer::GeometryChanged()
{
	FVector2f WidgetSize = GetPaintSpaceGeometry().GetLocalSize();
	FVector2f ImageSize = NonConstBrush->GetImageSize();

	if (WidgetSize.X < 1 || WidgetSize.Y < 1 || ImageSize.X < 1 || ImageSize.Y < 1)
	{
		return;
	}

	float WidgetAspect = float(WidgetSize.X) / WidgetSize.Y;
	float ImageAspect = float(ImageSize.X) / ImageSize.Y;

	float XRange, YRange;

	if (ImageAspect > WidgetAspect) // fit to width
	{
		XRange = 1.0;
		YRange = ImageAspect / WidgetAspect;
	}
	else // fit to height
	{
		XRange = WidgetAspect / ImageAspect;
		YRange = 1.0;
	}

	NonConstBrush->SetUVRegion(FBox2f(FVector2f(0.5 - XRange / 2, 0.5 - YRange / 2), FVector2f(0.5 + XRange / 2, 0.5 + YRange / 2)));
}

void SMetaHumanImageViewer::SetDrawBlanking(bool bInDrawBlanking)
{
	bDrawBlanking = bInDrawBlanking;
}
