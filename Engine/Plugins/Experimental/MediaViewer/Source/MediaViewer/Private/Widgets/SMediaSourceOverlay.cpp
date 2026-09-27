// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaSourceOverlay.h"

#include "DetailLayoutBuilder.h"
#include "ImageViewers/MediaSourceImageViewer.h"
#include "IMediaStreamPlayer.h"
#include "MediaPlayer.h"
#include "MediaPlayerEditorModule.h"
#include "MediaStream.h"
#include "MediaViewerStyle.h"
#include "Misc/FrameRate.h"
#include "Misc/Timecode.h"
#include "Misc/Timespan.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateIconFinder.h"
#include "Styling/StyleColors.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/MediaViewerDelegates.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SMediaViewer.h"
#include "Widgets/SMediaViewerTab.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMediaSourceOverlay"

namespace UE::MediaViewer::Private
{

constexpr double FadeTime = 2.0;

void SMediaSourceOverlay::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SMediaSourceOverlay::Construct(const FArguments& InArgs, const TSharedRef<FMediaSourceImageViewer>& InImageViewer,
	const TSharedPtr<SMediaViewerTab>& InViewerTab)
{
	ImageViewerWeak = InImageViewer;

	if (InViewerTab.IsValid())
	{
		Delegates = InViewerTab->GetViewer()->GetDelegates();
	}

	TrySetFrameRate();

	ChildSlot
	[
		SAssignNew(Container, SBox)
		[
			SNew(SBorder)
			.Padding(5.f)
			.BorderImage(FAppStyle::GetBrush("ToolTip.Background"))
			.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.75f))
			[
				SNew(SVerticalBox)		
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreateSlider()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.Padding(0.f, 2.f, 0.f, 0.f)
				[
					CreateControls()
				]
			]
		]
	];
}

void SMediaSourceOverlay::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

	if (!Container.IsValid())
	{
		return;
	}

	const bool bMouseOver = Delegates.IsValid() ? Delegates->IsOverViewer.Execute() : true;

	if (LastInteractionTime < 0 || bMouseOver)
	{
		LastInteractionTime = InCurrentTime;

		if (Container->GetVisibility() != EVisibility::Visible)
		{
			Container->SetVisibility(EVisibility::Visible);
		}
	}
	else if (InCurrentTime > (LastInteractionTime + FadeTime))
	{
		if (Container->GetVisibility() != EVisibility::Hidden)
		{
			Container->SetVisibility(EVisibility::Hidden);
		}
	}
}

UMediaStream* SMediaSourceOverlay::GetMediaStream() const
{
	if (TSharedPtr<FMediaSourceImageViewer> ImageViewer = ImageViewerWeak.Pin())
	{
		return ImageViewer->GetMediaStream();
	}

	return nullptr;
}

IMediaStreamPlayer* SMediaSourceOverlay::GetMediaStreamPlayer() const
{
	if (UMediaStream* MediaStream = GetMediaStream())
	{
		return MediaStream->GetPlayer().GetInterface();
	}

	return nullptr;
}

UMediaPlayer* SMediaSourceOverlay::GetMediaPlayer() const
{
	if (IMediaStreamPlayer* MediaStreamPlayer = GetMediaStreamPlayer())
	{
		return MediaStreamPlayer->GetPlayer();
	}

	return nullptr;
}

void SMediaSourceOverlay::TrySetFrameRate()
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		// INDEX_NONE here access the currently playing TrackIndex and FormatIndex.
		const float FrameRate = MediaPlayer->GetVideoTrackFrameRate(INDEX_NONE, INDEX_NONE);

		if (!FMath::IsNearlyZero(FrameRate))
		{
			FrameRateFloat = FrameRate;
		}
	}
}

TSharedRef<SWidget> SMediaSourceOverlay::CreateSlider()
{
	UMediaPlayer* MediaPlayer = GetMediaPlayer();

	if (!MediaPlayer)
	{
		return SNullWidget::NullWidget;
	}

	IMediaPlayerEditorModule* MediaPlayerEditorModule = FModuleManager::LoadModulePtr<IMediaPlayerEditorModule>("MediaPlayerEditor");

	if (!MediaPlayerEditorModule)
	{
		return SNullWidget::NullWidget;
	}

	TArray<TWeakObjectPtr<UMediaPlayer>> MediaPlayerList;
	MediaPlayerList.Add(MediaPlayer);

	const TSharedRef<IMediaPlayerSlider> MediaPlayerSlider = MediaPlayerEditorModule->CreateMediaPlayerSliderWidget(MediaPlayerList);

	MediaPlayerSlider->SetSliderHandleColor(FSlateColor(EStyleColor::AccentBlue));
	MediaPlayerSlider->SetVisibleWhenInactive(EVisibility::Visible);

	return MediaPlayerSlider;
}

TSharedRef<SWidget> SMediaSourceOverlay::CreateControls()
{
	return SNew(SHorizontalBox)

		// Current frame
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 5.f, 0.f)
		.FillWidth(1.f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FStyleColors::Foreground.GetSpecifiedColor())
			.ShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor())
			.ShadowOffset(FVector2D(1.f, 1.f))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &SMediaSourceOverlay::GetCurrentFrame)
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0.f, 0.f, 5.f, 0.f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FStyleColors::Foreground.GetSpecifiedColor())
			.ShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor())
			.ShadowOffset(FVector2D(1.f, 1.f))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(INVTEXT("/"))
		]

		// Total fames
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FStyleColors::Foreground.GetSpecifiedColor())
			.ShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor())
			.ShadowOffset(FVector2D(1.f, 1.f))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &SMediaSourceOverlay::GetTotalFrames)
		]

		// Rewind button.
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.IsEnabled(this, &SMediaSourceOverlay::Rewind_IsEnabled)
			.OnClicked(this, &SMediaSourceOverlay::Rewind_OnClicked)
			.ButtonStyle(FMediaViewerStyle::Get(), "MediaButtons")
			.ContentPadding(2.f)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Animation.Backward_End").GetIcon())
				.ToolTipText(LOCTEXT("Rewind", "Rewind the media to the beginning"))
				.DesiredSizeOverride(FVector2D(20.f))
			]
		]

		// Step back button.
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.IsEnabled(this, &SMediaSourceOverlay::StepBack_IsEnabled)
			.OnClicked(this, &SMediaSourceOverlay::StepBack_OnClicked)
			.ButtonStyle(FMediaViewerStyle::Get(), "MediaButtons")
			.ContentPadding(2.f)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Animation.Backward_Step").GetIcon())
				.ToolTipText(LOCTEXT("StepBack", "Step back 1 frame.\n\nOnly available while paused."))
				.DesiredSizeOverride(FVector2D(20.f))
			]
		]

		// Reverse button.
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.IsEnabled(this, &SMediaSourceOverlay::Reverse_IsEnabled)
			.OnClicked(this, &SMediaSourceOverlay::Reverse_OnClicked)
			.ButtonStyle(FMediaViewerStyle::Get(), "MediaButtons")
			.ContentPadding(2.f)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(this, &SMediaSourceOverlay::Reverse_GetBrush)
				.ToolTipText(this, &SMediaSourceOverlay::Reverse_GetToolTip)
				.DesiredSizeOverride(FVector2D(20.f))
			]
		]

		// Play button.
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.IsEnabled(this, &SMediaSourceOverlay::Play_IsEnabled)
			.OnClicked(this, &SMediaSourceOverlay::Play_OnClicked)
			.ButtonStyle(FMediaViewerStyle::Get(), "MediaButtons")
			.ContentPadding(2.f)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(this, &SMediaSourceOverlay::Play_GetBrush)
				.ToolTipText(this, &SMediaSourceOverlay::Play_GetToolTip)
				.DesiredSizeOverride(FVector2D(20.f))
			]
		]

		// Step forward button.
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.IsEnabled(this, &SMediaSourceOverlay::StepForward_IsEnabled)
			.OnClicked(this, &SMediaSourceOverlay::StepForward_OnClicked)
			.ButtonStyle(FMediaViewerStyle::Get(), "MediaButtons")
			.ContentPadding(2.f)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Animation.Forward_Step").GetIcon())
				.ToolTipText(LOCTEXT("StepForward", "Step forward 1 frame.\n\nOnly available while paused."))
				.DesiredSizeOverride(FVector2D(20.f))
			]
		]

		// Fast Forward button.
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.IsEnabled(this, &SMediaSourceOverlay::Forward_IsEnabled)
			.OnClicked(this, &SMediaSourceOverlay::Forward_OnClicked)
			.ButtonStyle(FMediaViewerStyle::Get(), "MediaButtons")
			.ContentPadding(2.f)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Animation.Forward_End").GetIcon())
				.ToolTipText(LOCTEXT("Forward", "Fast forward the media to the end."))
				.DesiredSizeOverride(FVector2D(20.f))
			]
		]

		// Current time
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(10.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FStyleColors::Foreground.GetSpecifiedColor())
			.ShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor())
			.ShadowOffset(FVector2D(1.f, 1.f))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &SMediaSourceOverlay::GetCurrentTime)
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(5.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FStyleColors::Foreground.GetSpecifiedColor())
			.ShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor())
			.ShadowOffset(FVector2D(1.f, 1.f))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(INVTEXT("/"))
		]

		// Total time
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5.f, 0.f, 0.f, 0.f)
		.FillWidth(1.f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FStyleColors::Foreground.GetSpecifiedColor())
			.ShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor())
			.ShadowOffset(FVector2D(1.f, 1.f))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &SMediaSourceOverlay::GetTotalTime)
		];
}

FText SMediaSourceOverlay::GetCurrentFrame() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		if (!FrameRateFloat.IsSet())
		{
			const_cast<SMediaSourceOverlay*>(this)->TrySetFrameRate();
		}

		if (FrameRateFloat.IsSet())
		{
			return FText::AsNumber(FMath::FloorToInt(MediaPlayer->GetTime().GetTotalSeconds() * static_cast<double>(FrameRateFloat.GetValue())) + 1);
		}
	}

	return INVTEXT("-");
}

FText SMediaSourceOverlay::GetTotalFrames() const
{
	if (!TotalFrames.IsSet())
	{
		if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
		{
			if (!FrameRateFloat.IsSet())
			{
				const_cast<SMediaSourceOverlay*>(this)->TrySetFrameRate();
			}

			if (FrameRateFloat.IsSet())
			{
				const FTimespan Duration = MediaPlayer->GetDuration();

				TotalFrames = FText::AsNumber(FMath::FloorToInt(Duration.GetTotalSeconds() * static_cast<double>(FrameRateFloat.GetValue())));
			}
		}
	}

	return TotalFrames.Get(INVTEXT("-"));
}

FText SMediaSourceOverlay::GetCurrentTime() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		if (!FrameRateFloat.IsSet())
		{
			const_cast<SMediaSourceOverlay*>(this)->TrySetFrameRate();
		}

		if (FrameRateFloat.IsSet())
		{
			// No straight conversion from float to FFrameRate, so estimate like this.
			FFrameRate FrameRate(FMath::RoundToInt(1000000.f * FrameRateFloat.GetValue()), 1000000);
			FTimecode Timecode(MediaPlayer->GetTime().GetTotalSeconds(), FrameRate, /* DropFrame */ false, /* Rollover */ false);

			return FText::FromString(Timecode.ToString());
		}
	}

	return INVTEXT("-");
}

FText SMediaSourceOverlay::GetTotalTime() const
{
	if (!TotalTime.IsSet())
	{
		if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
		{
			if (!FrameRateFloat.IsSet())
			{
				const_cast<SMediaSourceOverlay*>(this)->TrySetFrameRate();
			}

			if (FrameRateFloat.IsSet())
			{
				// No straight conversion from float to FFrameRate, so estimate like this.
				const FFrameRate FrameRate(FMath::RoundToInt(1000000.f * FrameRateFloat.GetValue()), 1000000);
				const FTimecode Timecode(MediaPlayer->GetDuration().GetTotalSeconds(), FrameRate, /* DropFrame */ false, /* Rollover */ false);

				TotalTime = FText::FromString(Timecode.ToString());
			}
		}
	}

	return TotalTime.Get(INVTEXT("-"));
}

bool SMediaSourceOverlay::Rewind_IsEnabled() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		return MediaPlayer->IsReady() &&
			MediaPlayer->SupportsSeeking() &&
			MediaPlayer->GetTime() > FTimespan::Zero();
	}

	return false;
}

FReply SMediaSourceOverlay::Rewind_OnClicked()
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		MediaPlayer->Pause();
		MediaPlayer->Rewind();
	}

	return FReply::Handled();
}

bool SMediaSourceOverlay::Reverse_IsEnabled() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		return MediaPlayer->IsReady() && MediaPlayer->SupportsRate(-1.f, /* Unthinned */ true);
	}

	return false;
}

const FSlateBrush* SMediaSourceOverlay::Reverse_GetBrush() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		const float Rate = FMath::Abs(MediaPlayer->GetRate());

		if (Rate < 0 && !FMath::IsNearlyZero(Rate))
		{
			return FAppStyle::Get().GetBrush("Animation.Pause");
		}
	}

	return FAppStyle::Get().GetBrush("Animation.Backward");
}

FText SMediaSourceOverlay::Reverse_GetToolTip() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		const float Rate = FMath::Abs(MediaPlayer->GetRate());

		if (Rate < 0 && !FMath::IsNearlyZero(Rate))
		{
			return LOCTEXT("Pause", "Pause media playback");
		}
	}

	return LOCTEXT("Reverse", "Play media in reverse.\n\nNot widely supported by media decoders.");
}

FReply SMediaSourceOverlay::Reverse_OnClicked()
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		const float Rate = FMath::Abs(MediaPlayer->GetRate());

		if (FMath::IsNearlyZero(Rate))
		{
			MediaPlayer->SetRate(-1.f);
		}
		else
		{
			MediaPlayer->Pause();
		}
	}

	return FReply::Handled();
}

bool SMediaSourceOverlay::StepBack_IsEnabled() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		const float Rate = FMath::Abs(MediaPlayer->GetRate());

		return MediaPlayer->IsReady() && MediaPlayer->IsPaused();
	}

	return false;
}

FReply SMediaSourceOverlay::StepBack_OnClicked()
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		const float FrameRate = MediaPlayer->GetVideoTrackFrameRate(INDEX_NONE, INDEX_NONE);

		if (!FMath::IsNearlyZero(FrameRate))
		{
			MediaPlayer->Seek(MediaPlayer->GetTime() - FTimespan::FromSeconds(1.f / FrameRate));
		}
	}

	return FReply::Handled();
}

bool SMediaSourceOverlay::Play_IsEnabled() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		return MediaPlayer->IsReady();
	}

	return false;
}

const FSlateBrush* SMediaSourceOverlay::Play_GetBrush() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		const float Rate = FMath::Abs(MediaPlayer->GetRate());

		if (Rate > 0 && !FMath::IsNearlyZero(Rate))
		{
			return FAppStyle::Get().GetBrush("Animation.Pause");
		}
	}

	return FAppStyle::Get().GetBrush("Animation.Forward");
}

FText SMediaSourceOverlay::Play_GetToolTip() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		const float Rate = FMath::Abs(MediaPlayer->GetRate());

		if (Rate > 0 && !FMath::IsNearlyZero(Rate))
		{
			return LOCTEXT("Pause", "Pause media playback");
		}
	}

	return LOCTEXT("Play", "Play media forward");
}

FReply SMediaSourceOverlay::Play_OnClicked()
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		const float Rate = FMath::Abs(MediaPlayer->GetRate());

		if (FMath::IsNearlyZero(Rate))
		{
			MediaPlayer->Play();
		}
		else
		{
			MediaPlayer->Pause();
		}
	}

	return FReply::Handled();
}

bool SMediaSourceOverlay::StepForward_IsEnabled() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		const float Rate = FMath::Abs(MediaPlayer->GetRate());

		return MediaPlayer->IsReady() && MediaPlayer->IsPaused();
	}

	return false;
}

FReply SMediaSourceOverlay::StepForward_OnClicked()
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		const float FrameRate = MediaPlayer->GetVideoTrackFrameRate(INDEX_NONE, INDEX_NONE);

		if (!FMath::IsNearlyZero(FrameRate))
		{
			MediaPlayer->Seek(MediaPlayer->GetTime() + FTimespan::FromSeconds(1.f / FrameRate));
		}
	}

	return FReply::Handled();
}

bool SMediaSourceOverlay::Forward_IsEnabled() const
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		return MediaPlayer->IsReady();
	}

	return false;
}

FReply SMediaSourceOverlay::Forward_OnClicked()
{
	if (UMediaPlayer* MediaPlayer = GetMediaPlayer())
	{
		// INDEX_NONE here access the currently playing TrackIndex and FormatIndex.
		const double FrameRate = MediaPlayer->GetVideoTrackFrameRate(INDEX_NONE, INDEX_NONE);

		if (!FMath::IsNearlyZero(FrameRate))
		{
			MediaPlayer->Pause();

			const double FrameTime = 1.0 / FrameRate;
			FTimespan SeekLocation = MediaPlayer->GetDuration();
			SeekLocation -= FTimespan::FromSeconds(FrameTime);

			MediaPlayer->Seek(SeekLocation);
		}
	}

	return FReply::Handled();
}

}

#undef LOCTEXT_NAMESPACE
