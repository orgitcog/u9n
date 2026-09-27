// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubtitlesSubsystem.h"

#include "Algo/RemoveIf.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Containers/Ticker.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"
#include "Styling/CoreStyle.h"
#include "SubtitlesAndClosedCaptionsModule.h"
#include "SubtitlesSettings.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtrTemplates.h"
#include "UObject/WeakObjectPtrTemplates.h"

// #SUBTITLES_PRD / #SUBTITLES_PRD_TODO - remove subtitles development comments prior to release

void USubtitlesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	BindDelegates();
}

void USubtitlesSubsystem::BindDelegates()
{
	if (TryCreateUMGWidget())
	{
		FSubtitlesAndClosedCaptionsDelegates::QueueSubtitle.BindUObject(this, &USubtitlesSubsystem::QueueSubtitle);
		FSubtitlesAndClosedCaptionsDelegates::IsSubtitleActive.BindUObject(this, &USubtitlesSubsystem::IsSubtitleActive);
		FSubtitlesAndClosedCaptionsDelegates::StopSubtitle.BindUObject(this, &USubtitlesSubsystem::StopSubtitle);
		FSubtitlesAndClosedCaptionsDelegates::StopAllSubtitles.BindUObject(this, &USubtitlesSubsystem::StopAllSubtitles);
	}
}


void USubtitlesSubsystem::QueueSubtitle(const FQueueSubtitleParameters& Params, const ESubtitleTiming Timing)
{
	constexpr float InfiniteDuration = FLT_MAX;
	const USubtitleAssetUserData& Subtitle = *CastChecked<const USubtitleAssetUserData>(&Params.Subtitle);
	UE_LOG(LogSubtitlesAndClosedCaptions, Display, TEXT("QueueSubtitle: '%s'"), *Subtitle.Text.ToString());


	// Externally-timed subtitles will be removed by the system queueing them, so they have an otherwise-infinite duration.
	float Duration = InfiniteDuration;
	if (Timing == ESubtitleTiming::InternallyTimed)
	{
		Duration = Params.Duration.IsSet() ? Params.Duration.GetValue() : Subtitle.Duration;
	}

	if (IsInGameThread())
	{
		AddActiveSubtitle(Subtitle, Duration);
	}
	else
	{
		ExecuteOnGameThread(
			TEXT("USubtitlesSubsystem::HandleQueueSubtitle"),
			[WeakThis = TWeakObjectPtr<USubtitlesSubsystem>(this), WeakSubtitle = TWeakObjectPtr<const USubtitleAssetUserData>(&Subtitle), Duration]()
			{
				if (TStrongObjectPtr<USubtitlesSubsystem> ThisPin = WeakThis.Pin())
				{
					if (TStrongObjectPtr<const USubtitleAssetUserData> SubtitlePin = WeakSubtitle.Pin())
					{
						ThisPin->AddActiveSubtitle(*SubtitlePin, Duration);
					}
				}
			}
		);
	}
}

void USubtitlesSubsystem::AddActiveSubtitle(const USubtitleAssetUserData& Subtitle, float Duration)
{
	// If the subtitle is already active then update its duration (by removing it then and then re-adding it)
	const FActiveSubtitle* FoundActiveSubtitle = ActiveSubtitles.FindByPredicate(
		[SubtitlePtr = &Subtitle](const FActiveSubtitle& ActiveSubtitle) { return ActiveSubtitle.Subtitle == SubtitlePtr; });
	if (FoundActiveSubtitle != nullptr)
	{
		RemoveActiveSubtitle(&Subtitle);
	}

	FActiveSubtitle NewActiveSubtitle{ &Subtitle };

	// Subtitles with delayed offset need a timer to await their entry in the queue.
	if (Subtitle.StartOffset > 0.f)
	{
		// The timer handle will be reused for the duration when it enters the active subtitle queue.
		// For now the timer tracks how long until it enters that queue.
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &USubtitlesSubsystem::MakeDelayedSubtitleActive, NewActiveSubtitle.Subtitle);
		GetWorldRef().GetTimerManager().SetTimer(NewActiveSubtitle.DurationTimerHandle, MoveTemp(Delegate), Subtitle.StartOffset, /*bLoop=*/false);

		DelayedSubtitles.Add(MoveTemp(NewActiveSubtitle));
	}
	else
	{
		// Without the delayed offset, instantly enter the queue as usual.
		// The timer here tracks how long until the subtitle will expire and leave the active subtitle queue.
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &USubtitlesSubsystem::RemoveActiveSubtitle, NewActiveSubtitle.Subtitle);
		Duration = FMath::Max(Duration, SubtitleMinDuration);
		GetWorldRef().GetTimerManager().SetTimer(NewActiveSubtitle.DurationTimerHandle, MoveTemp(Delegate), Duration, /*bLoop=*/false);

		ActiveSubtitles.Add(MoveTemp(NewActiveSubtitle));
		ActiveSubtitles.StableSort([](const FActiveSubtitle& Lhs, const FActiveSubtitle& Rhs) { return Lhs.Subtitle->Priority > Rhs.Subtitle->Priority; });

		UpdateWidgetData();
	}
}

void USubtitlesSubsystem::MakeDelayedSubtitleActive(TObjectPtr<const USubtitleAssetUserData> Subtitle)
{
	if (IsValid(Subtitle))
	{
		FActiveSubtitle* DelayedSubtitle = Algo::FindByPredicate(DelayedSubtitles, [&Subtitle](const FActiveSubtitle& DelayedSubtitle) {return DelayedSubtitle.Subtitle == Subtitle; });

		if (DelayedSubtitle != nullptr)
		{
			const USubtitleAssetUserData& SubtitleAsset = *DelayedSubtitle->Subtitle;
			const float Duration = FMath::Max(SubtitleAsset.Duration, SubtitleMinDuration);

			// Reuse the Timer Handle for duration, now that it's no longer needed for the delay.
			FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &USubtitlesSubsystem::RemoveActiveSubtitle, DelayedSubtitle->Subtitle);
			GetWorldRef().GetTimerManager().SetTimer(DelayedSubtitle->DurationTimerHandle, MoveTemp(Delegate), Duration, /*bLoop=*/false);

			// Insert the new subtitle to the actual queue and ensure it remains sorted by priority.
			ActiveSubtitles.Add(MoveTemp(*DelayedSubtitle));
			ActiveSubtitles.StableSort([](const FActiveSubtitle& Lhs, const FActiveSubtitle& Rhs) { return Lhs.Subtitle->Priority > Rhs.Subtitle->Priority; });

			// Remove from the list of Delayed Subtitles
			const int32 FirstRemovedIndex = Algo::RemoveIf(DelayedSubtitles, [&Subtitle](const FActiveSubtitle& DelayedSubtitle) {return DelayedSubtitle.Subtitle == Subtitle; });
			DelayedSubtitles.SetNum(FirstRemovedIndex);

			UpdateWidgetData();
		}
	}
}

bool USubtitlesSubsystem::IsSubtitleActive(const UAssetUserData& Data) const
{
	if (!ensureMsgf(IsInGameThread(), TEXT("IsSubtitleActive must currently be run on the GameThread - ActiveSubtitles vector is not locked")))
	{
		return false;
	}

	const USubtitleAssetUserData* Subtitle = CastChecked<const USubtitleAssetUserData>(&Data);

	const FActiveSubtitle* FoundActiveSubtitle = ActiveSubtitles.FindByPredicate(
		[Subtitle](const FActiveSubtitle& ActiveSubtitle) { return ActiveSubtitle.Subtitle == Subtitle; });

	return FoundActiveSubtitle != nullptr;
}

void USubtitlesSubsystem::StopSubtitle(const UAssetUserData& Data)
{
	const USubtitleAssetUserData* Subtitle = CastChecked<const USubtitleAssetUserData>(&Data);
	RemoveActiveSubtitle(Subtitle);
}

void USubtitlesSubsystem::StopAllSubtitles()
{
	// Clean up queued subtitles.
	FTimerManager& TimerManager = GetWorldRef().GetTimerManager();
	for (FActiveSubtitle& ActiveSubtitle : ActiveSubtitles)
	{
		TimerManager.ClearTimer(ActiveSubtitle.DurationTimerHandle);
	}
	ActiveSubtitles.Empty();

	// Also remove delayed-start subtitles not yet in the queue.
	for (FActiveSubtitle& DelayedSubtitle : DelayedSubtitles)
	{
		TimerManager.ClearTimer(DelayedSubtitle.DurationTimerHandle);
	}
	DelayedSubtitles.Empty();

	// Clear the widget's display
	if (IsValid(SubtitleWidget))
	{
		SubtitleWidget->StopDisplayingSubtitle(ESubtitleType::AudioDescription);
		SubtitleWidget->StopDisplayingSubtitle(ESubtitleType::ClosedCaption);
		SubtitleWidget->StopDisplayingSubtitle(ESubtitleType::Subtitle);
	}
	else 
	{
		UE_LOG(LogSubtitlesAndClosedCaptions, Warning, TEXT("Can't remove subtitles because there isn't a valid UMG widget."));
	}
}

void USubtitlesSubsystem::RemoveActiveSubtitle(TObjectPtr<const USubtitleAssetUserData> Subtitle)
{
	bool bSuccessfullyRemoved = false;
	int32 FirstRemovedIndex = Algo::StableRemoveIf(ActiveSubtitles, [&Subtitle](const FActiveSubtitle& ActiveSubtitle) {return ActiveSubtitle.Subtitle == Subtitle; });

	FTimerManager& TimerManager = GetWorldRef().GetTimerManager();
	for (int32 Index = FirstRemovedIndex; Index < ActiveSubtitles.Num(); ++Index)
	{
		TimerManager.ClearTimer(ActiveSubtitles[Index].DurationTimerHandle);
		bSuccessfullyRemoved = true;
	}

	ActiveSubtitles.SetNum(FirstRemovedIndex);

	// Stop Displaying the removed subtitle and display a newly-most-relevant one if applicable.
	if (bSuccessfullyRemoved && IsValid(SubtitleWidget))
	{
		SubtitleWidget->StopDisplayingSubtitle(Subtitle->SubtitleType);

		if (!ActiveSubtitles.IsEmpty())
		{
			const USubtitleAssetUserData& HighestPrioritySubtitle = *ActiveSubtitles[0].Subtitle;
			SubtitleWidget->StartDisplayingSubtitle(HighestPrioritySubtitle);
		}
	}
	else if(!IsValid(SubtitleWidget))
	{
		UE_LOG(LogSubtitlesAndClosedCaptions, Warning, TEXT("Can't remove subtitles because there isn't a valid UMG widget."));
	}

	// Also remove delayed-start subtitles using this asset.
	FirstRemovedIndex = Algo::RemoveIf(DelayedSubtitles, [&Subtitle](const FActiveSubtitle& DelayedSubtitle) {return DelayedSubtitle.Subtitle == Subtitle; });

	for (int32 Index = FirstRemovedIndex; Index < DelayedSubtitles.Num(); ++Index)
	{
		TimerManager.ClearTimer(DelayedSubtitles[Index].DurationTimerHandle);
	}
	DelayedSubtitles.SetNum(FirstRemovedIndex);
}

bool USubtitlesSubsystem::TryCreateUMGWidget()
{
	// Set up the UMG widget
	const USubtitlesSettings* Settings = GetDefault<USubtitlesSettings>();
	check(Settings != nullptr);
	const TSubclassOf<USubtitleTextBlock>& WidgetToUse = Settings->GetWidget();
	if (IsValid(WidgetToUse))
	{
		SubtitleWidget = CreateWidget<USubtitleTextBlock>(GetWorld(), WidgetToUse);
	}
	else
	{
		// Fallback to default widget (not set by user):
		const TSubclassOf<USubtitleTextBlock>& WidgetToUseDefault = Settings->GetWidgetDefault();
		if (IsValid(WidgetToUseDefault))
		{
			SubtitleWidget = CreateWidget<USubtitleTextBlock>(GetWorld(), WidgetToUseDefault);
		}
	}
	bInitializedWidget = false;

	return IsValid(SubtitleWidget);
}

void USubtitlesSubsystem::UpdateWidgetData()
{
	// Update the widget. If it's not valid (eg, destroyed on non-seamless travel), try re-creating it first.
	if (IsValid(SubtitleWidget) || (bInitializedWidget && TryCreateUMGWidget() && IsValid(SubtitleWidget)))
	{
		if (!bInitializedWidget)
		{
			SubtitleWidget->AddToViewport();
			SubtitleWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			bInitializedWidget = true;
		}

		const USubtitleAssetUserData& NewHighestPrioritySubtitle = *ActiveSubtitles[0].Subtitle;
		SubtitleWidget->StartDisplayingSubtitle(NewHighestPrioritySubtitle);
	}
	else
	{
		UE_LOG(LogSubtitlesAndClosedCaptions, Warning, TEXT("Can't display subtitles because there isn't a valid UMG widget to display it to (check Project Settings)."));
	}
}

