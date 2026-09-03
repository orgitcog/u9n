// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rendering/DrawElements.h"
#include "UObject/Class.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Interfaces/IPluginManager.h"
#include "Brushes/SlateImageBrush.h"



template<class T>
class SMetaHumanOverlayWidget : public T
{

public:

	SMetaHumanOverlayWidget()
	{
		NoEntryBrush = FSlateVectorImageBrush(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir() + TEXT("/Icons/ImageViewerNoEntry_20.svg"), FVector2D(20, 20));
	}

	virtual int32 OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry,
		const FSlateRect& InWidgetClippingRect, FSlateWindowElementList& OutDrawElements,
		int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const override
	{
		int32 LayerId = T::OnPaint(InArgs, InAllottedGeometry, InWidgetClippingRect, OutDrawElements, InLayerId, InWidgetStyle, InParentEnabled);

		if (!Overlay.IsEmpty())
		{
			const FSlateColorBrush Brush = FSlateColorBrush(FLinearColor::White);
			const FLinearColor Colour = FLinearColor(0, 0, 0, 0.5);

			const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", 12);
			const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

			const FVector2D TextBB = FontMeasureService->Measure(Overlay, FontInfo);

			const FGeometry Box = InAllottedGeometry.MakeChild(TextBB, FSlateLayoutTransform(FVector2D((InAllottedGeometry.GetLocalSize().X - TextBB.X) / 2 + 15, InAllottedGeometry.GetLocalSize().Y - TextBB.Y - 10)));

			LayerId++;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, InAllottedGeometry.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, Colour);

			LayerId++;
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, Box.ToPaintGeometry(), Overlay, FontInfo);

			const FGeometry NoEntryBox = InAllottedGeometry.MakeChild(FVector2D(20, 20), FSlateLayoutTransform(FVector2D((InAllottedGeometry.GetLocalSize().X - TextBB.X) / 2 - 15, InAllottedGeometry.GetLocalSize().Y - TextBB.Y - 12)));

			LayerId++;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, NoEntryBox.ToPaintGeometry(), &NoEntryBrush, ESlateDrawEffect::None, FLinearColor(1, 1, 1));
		}

		return LayerId;
	}

	void SetOverlay(const FText& InOverlay)
	{
		Overlay = InOverlay;
	}

private:

	FText Overlay;
	FSlateBrush NoEntryBrush;
};
