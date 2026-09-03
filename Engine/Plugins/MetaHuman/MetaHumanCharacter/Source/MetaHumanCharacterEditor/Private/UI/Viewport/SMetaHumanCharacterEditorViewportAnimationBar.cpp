// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorViewportAnimationBar.h"

#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Math/Color.h"
#include "Widgets/Images/SImage.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterEditorViewportClient.h"
#include "MetaHumanCharacterEditorActorInterface.h"
#include "MetaHumanCharacterAnimInstance.h"
#include "Widgets/Input/SSpinBox.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "SScrubControlPanel.h"
#include "ITransportControl.h" 
#include "SWarningOrErrorBox.h"
#include "Widgets/Text/SRichTextBlock.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"


void SMetaHumanCharacterEditorViewportAnimationBar::Construct(const FArguments& InArgs)
{
	ViewportClient = InArgs._AnimationBarViewportClient;
	bAnimationPlaying = false;
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Menu.Background"))
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
		.Padding(FMargin(2.0, 1.0, 2.0, 1.0))
		[
			MakeAnimationBarScrubber()
		]
	];
}

AMetaHumanInvisibleDrivingActor* SMetaHumanCharacterEditorViewportAnimationBar::GetInvisibleDrivingActor() const
{
	UMetaHumanCharacterEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	if (EditorSubsystem)
	{
		if (UMetaHumanCharacter* Character = ViewportClient->WeakCharacter.Get())
		{
			EMetaHumanCharacterRigState State = EditorSubsystem->GetRiggingState(Character);
			if (State != EMetaHumanCharacterRigState::Rigged)
			{
				return nullptr;
			}
			return EditorSubsystem->GetInvisibleDrivingActor(Character);
		}
	}

	return nullptr;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorViewportAnimationBar::MakeAnimationBarScrubber()
{
	TArray<FTransportControlWidget> TransportControlWidgets;

	TransportControlWidgets.Add(FTransportControlWidget(ETransportControlWidgetType::BackwardPlay));
	TransportControlWidgets.Add(FTransportControlWidget(ETransportControlWidgetType::ForwardPlay));

	const FTransportControlWidget NewWidget(FOnMakeTransportWidget::CreateSP(this, &SMetaHumanCharacterEditorViewportAnimationBar::OnCreateStopButtonWidget));
	TransportControlWidgets.Add(NewWidget);

	return SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SScrubControlPanel)
				.IsEnabled(this, &SMetaHumanCharacterEditorViewportAnimationBar::IsScrubWidgetEnabled)
				.bDisplayAnimScrubBarEditing(true)
				.Clipping(EWidgetClipping::ClipToBounds)
				.Value(this, &SMetaHumanCharacterEditorViewportAnimationBar::GetScrubValue)
				.NumOfKeys_Lambda([this]() {return this->GetNumberOfKeys(); })
				.SequenceLength_Lambda([this]() {return this->GetSequenceLength(); })
				.DisplayDrag(true)
				.OnValueChanged(this, &SMetaHumanCharacterEditorViewportAnimationBar::OnValueChanged)
				.OnBeginSliderMovement(this, &SMetaHumanCharacterEditorViewportAnimationBar::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &SMetaHumanCharacterEditorViewportAnimationBar::OnEndSliderMovement)
				.OnClickedForwardPlay(this, &SMetaHumanCharacterEditorViewportAnimationBar::OnClick_Forward)
				.OnClickedBackwardPlay(this, &SMetaHumanCharacterEditorViewportAnimationBar::OnClick_Backward)
				.OnGetPlaybackMode(this, &SMetaHumanCharacterEditorViewportAnimationBar::GetPlaybackMode)
				.ViewInputMin(0)
				.ViewInputMax_Lambda([this]() -> float
					{
						if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
						{
							return InvisibleDrivingActor->GetAnimationLength();
						}
						return 0.f;
					})
				.bAllowZoom(false)
				.IsRealtimeStreamingMode(false)
				.TransportControlWidgetsToCreate(TransportControlWidgets)
		]

		+ SOverlay::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			// warning
			SNew(SBox)
				.Padding(2.f)
				[
					SNew(SRichTextBlock)
						.AutoWrapText(false)
						.Visibility(this, &SMetaHumanCharacterEditorViewportAnimationBar::WarningVisibility)
						.Text(LOCTEXT("AnimationDisableWhenUnrigged", "Playback disabled on unrigged Assets. Create a Rig to enable"))
				]
		];


}

EPlaybackMode::Type SMetaHumanCharacterEditorViewportAnimationBar::GetPlaybackMode() const
{
	if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
	{
		switch(InvisibleDrivingActor->GetAnimationPlayState())
		{
			case EMetaHumanCharacterAnimationPlayState::PlayingForward :
				return EPlaybackMode::PlayingForward;
				break;
			case EMetaHumanCharacterAnimationPlayState::PlayingBackwards:
				return EPlaybackMode::PlayingReverse;
				break;
			case EMetaHumanCharacterAnimationPlayState::Paused:
				return EPlaybackMode::Stopped;
				break;
			default:
				return EPlaybackMode::Stopped;
		}
	}
	return EPlaybackMode::Type();
}

float SMetaHumanCharacterEditorViewportAnimationBar::GetScrubValue() const
{
	if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
	{
		return InvisibleDrivingActor->GetCurrentPlayTime();
	}

	return 0.f;
}

bool SMetaHumanCharacterEditorViewportAnimationBar::IsScrubWidgetEnabled() const
{
	if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
	{
		return (InvisibleDrivingActor->GetPreviewAnimInstance() != nullptr);
	}
	return false;
}

EVisibility SMetaHumanCharacterEditorViewportAnimationBar::WarningVisibility() const
{
	return !IsScrubWidgetEnabled() ? EVisibility::Visible : EVisibility::Collapsed;;
}


uint32 SMetaHumanCharacterEditorViewportAnimationBar::GetNumberOfKeys() const
{
	int32 NumKeys = 0;
	if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
	{
		NumKeys = InvisibleDrivingActor->GetNumberOfAnimationKeys();
	}
	
	return NumKeys;
}

float SMetaHumanCharacterEditorViewportAnimationBar::GetSequenceLength() const
{
	if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
	{
		return InvisibleDrivingActor->GetAnimationLength();
	}
	return 0.0f;
}

void SMetaHumanCharacterEditorViewportAnimationBar::OnValueChanged(float NewValue)
{
	if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
	{
		InvisibleDrivingActor->ScrubAnimation(NewValue);
	}
}

void SMetaHumanCharacterEditorViewportAnimationBar::OnBeginSliderMovement()
{
	if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
	{
		InvisibleDrivingActor->BeginAnimationScrubbing();
	}
}

void SMetaHumanCharacterEditorViewportAnimationBar::OnEndSliderMovement(float NewValue)
{
	if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
	{
		InvisibleDrivingActor->EndAnimationScrubbing();
	}
}

FReply SMetaHumanCharacterEditorViewportAnimationBar::OnClick_Forward()
{
	if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
	{
		switch (InvisibleDrivingActor->GetAnimationPlayState())
		{
		case EMetaHumanCharacterAnimationPlayState::PlayingForward:
			InvisibleDrivingActor->PauseAnimation();
			break;
		case EMetaHumanCharacterAnimationPlayState::PlayingBackwards:
			InvisibleDrivingActor->PlayAnimation();
			break;
		case EMetaHumanCharacterAnimationPlayState::Paused:
			InvisibleDrivingActor->PlayAnimation();
			break;
		}
	}
	return FReply::Handled();
}

FReply SMetaHumanCharacterEditorViewportAnimationBar::OnClick_Backward()
{
	if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
	{
		switch (InvisibleDrivingActor->GetAnimationPlayState())
		{
		case EMetaHumanCharacterAnimationPlayState::PlayingForward:
			InvisibleDrivingActor->PlayAnimationReverse();
			break;
		case EMetaHumanCharacterAnimationPlayState::PlayingBackwards:
			InvisibleDrivingActor->PauseAnimation();
			break;
		case EMetaHumanCharacterAnimationPlayState::Paused:
			InvisibleDrivingActor->PlayAnimationReverse();
			break;
		}
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SMetaHumanCharacterEditorViewportAnimationBar::OnCreateStopButtonWidget()
{
	TSharedPtr<SButton> StopButton = SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.IsFocusable(false)
		.ContentPadding(0.0f)
		.OnClicked_Lambda([this]()
		{
			if (AMetaHumanInvisibleDrivingActor* InvisibleDrivingActor = GetInvisibleDrivingActor())
			{
				{
					InvisibleDrivingActor->StopAnimation();
					bAnimationPlaying = false;
				}
			}

			return FReply::Handled();
		})
		.ToolTipText(LOCTEXT("AnimationStopButtonTooltip", "Stop"));

		StopButton->SetContent(SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush("Viewport.AnimationBar.Stop"))
		);

		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				StopButton.ToSharedRef()
			];
}

#undef LOCTEXT_NAMESPACE