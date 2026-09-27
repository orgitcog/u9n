// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SLeafWidget.h"

#define UE_API SLATEIM_API

struct FSlateIMLineGraphData
{
	TArray<FVector2D> NormalizedPoints;
	FLinearColor Color = FLinearColor::White;
	float LineThickness = 1.0f;
	FDoubleRange XViewRange;
	FDoubleRange YViewRange;
	bool bIsStale = false;
};

class SImGraph : public SLeafWidget
{
	SLATE_DECLARE_WIDGET(SImGraph, SLeafWidget)
	
public:
	SLATE_BEGIN_ARGS(SImGraph)
		{
		}

	SLATE_END_ARGS()

	UE_API void Construct(const FArguments& InArgs);

	UE_API void BeginGraph();
	UE_API void EndGraph();

	UE_API void AddLineGraph(const TArrayView<FVector2D>& Points, const FLinearColor& LineColor, float LineThickness, const FDoubleRange& XViewRange, const FDoubleRange& YViewRange);
	UE_API void AddLineGraph(const TArrayView<double>& Points, const FLinearColor& LineColor, float LineThickness, const FDoubleRange& ViewRange);

private:
	UE_API virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	UE_API virtual FVector2D ComputeDesiredSize(float) const override;

	FSlateIMLineGraphData& GetNextLineGraph(const FLinearColor& LineColor, float LineThickness, const FDoubleRange& XViewRange, const FDoubleRange& YViewRange);
	
	TArray<FSlateIMLineGraphData> LineGraphs;
};

#undef UE_API
