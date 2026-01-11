// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageViewer/MediaImageViewer.h"

#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Actor.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector2D.h"
#include "Misc/TVariant.h"
#include "Widgets/SMediaViewer.h"

namespace UE::MediaViewer
{

const FSlateColorBrush FMediaImageViewer::BackgroundColorBrush(FLinearColor::White);

MEDIAVIEWER_API FText FMediaImageViewer::GetObjectDisplayName(const UObject* InObject)
{
	if (!InObject)
	{
		return FText::GetEmpty();
	}

	if (AActor* Actor = InObject->GetTypedOuter<AActor>())
	{
		const FString ActorLabel = Actor->GetActorLabel(/* Create if none */ false);

		if (ActorLabel.IsEmpty())
		{
			return FText::FromName(Actor->GetFName());
		}

		return FText::FromString(ActorLabel);
	}

	return FText::FromName(InObject->GetFName());
}

FMediaImageViewer::FMediaImageViewer(const FMediaImageViewerInfo& InImageInfo)
	: ImageInfo(InImageInfo)
	, BackgroundImageBrush(static_cast<UObject*>(nullptr), FVector2D(1))
	, Brush(nullptr)
{
}

void FMediaImageViewer::UpdateId(const FGuid& InId)
{
	ImageInfo.Id = InId;
}

void FMediaImageViewer::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged)
{
	if (UTexture* Texture = PanelSettings.BackgroundTexture.LoadSynchronous())
	{
		BackgroundImageBrush.SetResourceObject(Texture);
		BackgroundImageBrush.ImageSize.X = Texture->GetSurfaceWidth();
		BackgroundImageBrush.ImageSize.Y = Texture->GetSurfaceHeight();
	}
	else
	{
		BackgroundImageBrush.SetResourceObject(nullptr);
	}
}

FVector2D FMediaImageViewer::GetViewerCenter(const FVector2D& InViewerSize) const
{
	return InViewerSize * 0.5;
}

FVector2D FMediaImageViewer::GetPaintOffsetForViewerCenter(const FVector2D& InViewerSize) const
{
	return GetViewerCenter(InViewerSize) - (FVector2D(ImageInfo.Size.X, ImageInfo.Size.Y) * 0.5 * GetPaintSettings().Scale);
}

FVector2D FMediaImageViewer::GetPaintOffset(const FVector2D& InViewerSize, const FVector2D& InViewerPosition) const
{
	return GetPaintOffsetForViewerCenter(InViewerSize) + (FVector2D(GetPaintSettings().Offset)) + InViewerPosition;
}

FVector2D FMediaImageViewer::GetPaintSize() const
{
	return FVector2D(ImageInfo.Size.X, ImageInfo.Size.Y) * GetPaintSettings().Scale;
}

FSlateClippingZone FMediaImageViewer::CreateSlateClippingZone(const FSlateRect& InCullingRect, float InDPIScale, 
	const FVector2D& InViewerPosition, EOrientation InOrientation, const FFloatRange& InUVRange) const
{
	constexpr float WindowBorderPadding = 4.f;

	const float Left = InCullingRect.Left + InViewerPosition.X * InDPIScale + WindowBorderPadding;
	const float Right = InCullingRect.Right - WindowBorderPadding;
	const float Top = InCullingRect.Top + InViewerPosition.Y * InDPIScale + WindowBorderPadding;
	const float Bottom = InCullingRect.Bottom - WindowBorderPadding;

	FSlateClippingZone ClippingZone;

	switch (InOrientation)
	{
		case Orient_Horizontal:
			ClippingZone.TopLeft.Y = Top;
			ClippingZone.TopRight.Y = Top;
			ClippingZone.BottomLeft.Y = Bottom;
			ClippingZone.BottomRight.Y = Bottom;

			ClippingZone.TopLeft.X = FMath::Lerp(Left, Right, InUVRange.GetLowerBound().GetValue());
			ClippingZone.TopRight.X = FMath::Lerp(Left, Right, InUVRange.GetUpperBound().GetValue());
			ClippingZone.BottomLeft.X = ClippingZone.TopLeft.X;
			ClippingZone.BottomRight.X = ClippingZone.TopRight.X;
			break;

		case Orient_Vertical:
			ClippingZone.TopLeft.Y = FMath::Lerp(Top, Bottom, InUVRange.GetLowerBound().GetValue());
			ClippingZone.BottomLeft.Y = FMath::Lerp(Top, Bottom, InUVRange.GetUpperBound().GetValue());
			ClippingZone.TopRight.Y = ClippingZone.TopLeft.Y;
			ClippingZone.BottomRight.Y = ClippingZone.BottomLeft.Y;

			ClippingZone.TopLeft.X = Left;
			ClippingZone.BottomLeft.X = Left;
			ClippingZone.TopRight.X = Right;
			ClippingZone.BottomRight.X = Right;
			break;
	}

	return ClippingZone;
}

void FMediaImageViewer::Paint(FMediaImagePaintParams& InPaintParams)
{
	FVector2D PaintOffset = GetPaintOffset(InPaintParams.ViewerSize, InPaintParams.ViewerPosition);
	FVector2D PaintSize = GetPaintSize();
	FPaintGeometry PaintGeometry = InPaintParams.AllottedGeometry.ToPaintGeometry(PaintSize, FSlateLayoutTransform(PaintOffset));

	const FMediaImagePaintGeometry Geometry = {
		MoveTemp(PaintOffset),
		MoveTemp(PaintSize),
		MoveTemp(PaintGeometry)
	};

	const FSlateClippingZone ClippingZone = CreateSlateClippingZone(
		InPaintParams.MyCullingRect,
		InPaintParams.DPIScale,
		InPaintParams.ViewerPosition,
		InPaintParams.Orientation,
		InPaintParams.UVRange
	);

	InPaintParams.DrawElements.PushClip(ClippingZone);

	PaintPanel(InPaintParams, Geometry);
	PaintImage(InPaintParams, Geometry);

	InPaintParams.DrawElements.PopClip();
}

void FMediaImageViewer::PaintPanel(FMediaImagePaintParams& InPaintParams, const FMediaImagePaintGeometry& InPaintGeometry)
{
	const FLinearColor AlphaTint = FLinearColor(1.f, 1.f, 1.f, InPaintParams.ImageOpacity);

	if (PanelSettings.BackgroundColor.IsSet())
	{
		FSlateDrawElement::MakeBox(
			InPaintParams.DrawElements,
			InPaintParams.LayerId,
			InPaintGeometry.Geometry,
			&BackgroundColorBrush,
			ESlateDrawEffect::NoPixelSnapping,
			PanelSettings.BackgroundColor.GetValue() * AlphaTint
		);

		++InPaintParams.LayerId;
	}

	if (BackgroundImageBrush.GetResourceObject())
	{
		FSlateDrawElement::MakeBox(
			InPaintParams.DrawElements,
			InPaintParams.LayerId,
			InPaintGeometry.Geometry,
			&BackgroundImageBrush,
			ESlateDrawEffect::NoPixelSnapping,
			AlphaTint
		);

		++InPaintParams.LayerId;
	}
}

void FMediaImageViewer::PaintImage(FMediaImagePaintParams& InPaintParams, const FMediaImagePaintGeometry& InPaintGeometry)
{
	if (!Brush.IsValid())
	{
		return;
	}

	FSlateDrawElement::MakeBox(
		InPaintParams.DrawElements,
		InPaintParams.LayerId,
		InPaintGeometry.Geometry,
		Brush.Get(),
		DrawEffects,
		GetPaintSettings().Tint * FLinearColor(1.f, 1.f, 1.f, InPaintParams.ImageOpacity)
	);

	++InPaintParams.LayerId;
}

} // UE::MediaViewer
