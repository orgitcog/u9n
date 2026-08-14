// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Internationalization/Text.h"
#include "Misc/Optional.h"

class IMediaStreamPlayer;
class SBox;
class UMediaPlayer;
class UMediaStream;
struct FSlateBrush;

namespace UE::MediaViewer
{
	class SMediaViewerTab;
}

namespace UE::MediaViewer::Private
{

class FMediaSourceImageViewer;
struct FMediaViewerDelegates;
	
class SMediaSourceOverlay : public SCompoundWidget
{
	SLATE_DECLARE_WIDGET(SMediaSourceOverlay, SCompoundWidget)

	SLATE_BEGIN_ARGS(SMediaSourceOverlay)
		{}
	SLATE_END_ARGS()

public:
	virtual ~SMediaSourceOverlay() override = default;

	void Construct(const FArguments& InArgs, const TSharedRef<FMediaSourceImageViewer>& InImageViewer, 
		const TSharedPtr<SMediaViewerTab>& InViewerTab);

	//~ Begin SWidget
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget

protected:
	TWeakPtr<FMediaSourceImageViewer> ImageViewerWeak;
	TSharedPtr<FMediaViewerDelegates> Delegates;
	TSharedPtr<SBox> Container;
	TOptional<float> FrameRateFloat;
	mutable TOptional<FText> TotalFrames;
	mutable TOptional<FText> TotalTime;

	/** The last time the mouse was moved over the widget. */
	double LastInteractionTime = -1.0;

	UMediaStream* GetMediaStream() const;

	IMediaStreamPlayer* GetMediaStreamPlayer() const;

	UMediaPlayer* GetMediaPlayer() const;

	void TrySetFrameRate();

	TSharedRef<SWidget> CreateSlider();

	TSharedRef<SWidget> CreateControls();

	FText GetCurrentFrame() const;
	FText GetTotalFrames() const;

	FText GetCurrentTime() const;
	FText GetTotalTime() const;

	bool Rewind_IsEnabled() const;
	FReply Rewind_OnClicked();

	bool Reverse_IsEnabled() const;
	const FSlateBrush* Reverse_GetBrush() const;
	FText Reverse_GetToolTip() const;
	FReply Reverse_OnClicked();

	bool StepBack_IsEnabled() const;
	FReply StepBack_OnClicked();

	bool Play_IsEnabled() const;
	const FSlateBrush* Play_GetBrush() const;
	FText Play_GetToolTip() const;
	FReply Play_OnClicked();

	bool StepForward_IsEnabled() const;
	FReply StepForward_OnClicked();

	bool Forward_IsEnabled() const;
	FReply Forward_OnClicked();
};

} // UE::MediaViewer::Private