// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Input/Events.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::Insights::TimingProfiler
{

class FTimeMarker;
class STimingView;

FUNC_DECLARE_DELEGATE(FOnGetTimingView, TSharedPtr<STimingView>, TSharedRef<FTimeMarker>);
FUNC_DECLARE_DELEGATE(FOnTimeMarkerChanged, void, TSharedRef<FTimeMarker>);

class STimeMarkerEditBlock : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STimeMarkerEditBlock) {}
		SLATE_ARGUMENT(FMargin, Padding)
		SLATE_ATTRIBUTE(TSharedPtr<FTimeMarker>, PreviousTimeMarker)
		SLATE_EVENT(FOnGetTimingView, OnGetTimingView)
		SLATE_EVENT(FOnTimeMarkerChanged, OnTimeMarkerChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FTimeMarker> InTimeMarker);

private:
	TSharedPtr<STimingView> GetTimingView() const;
	void OnTimeMarkerChanged();
	FReply OnLabelDoubleClicked(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent);

private:
	TSharedPtr<FTimeMarker> TimeMarker;

	FMargin Padding;

	/** The previous TimeMarker; used to display delta time. */
	TAttribute<TSharedPtr<FTimeMarker>> PreviousTimeMarker;

	/** Called whenever this widget needs to know the TimingView widget where the TimeMarker is located. */
	FOnGetTimingView OnGetTimingViewCallback;

	/** Called whenever the time marker is changed by this widget. */
	FOnTimeMarkerChanged OnTimeMarkerChangedCallback;
};

} // namespace UE::Insights::TimingProfiler
