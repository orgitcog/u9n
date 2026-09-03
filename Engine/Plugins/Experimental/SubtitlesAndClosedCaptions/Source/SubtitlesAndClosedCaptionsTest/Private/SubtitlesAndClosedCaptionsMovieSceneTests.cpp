// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "MovieSceneSubtitlesSystem.h"
#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"
#include "SubtitlesSubsystem.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "TimerManager.h"

#include "SubtitlesAndClosedCaptions/Public/MovieSceneSubtitlesSystem.h" // included to test internal FEvaluateSubtitles struct

#if 0 // Temporarily disabling these tests as they have a dangling reference that trips up the static analysis on certain build configurations.
#if WITH_DEV_AUTOMATION_TESTS

using namespace UE::MovieScene;

namespace SubtitlesAndClosedCaptions::Test
{
	const FFrameRate TestFrameRate(30, 1);
	constexpr float SubtitleDurationSeconds = 4.f;
	constexpr int SubtitleDurationFrames = SubtitleDurationSeconds * 30;

	// Helper struct for initializing tests so the same thing doesn't have to be done in every class
	struct FMovieSceneSubtitlesTest
	{
		FMovieSceneSubtitlesTest()
			: World(NewObject<UWorld>(GetTransientPackage(), NAME_None, RF_Transient))
			, Subsystem(NewObject<USubtitlesSubsystem>(World, NAME_None, RF_Transient))
			, Subtitle(NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient))
			, Section(NewObject<UMovieSceneSubtitleSection>(GetTransientPackage(), NAME_None, RF_Transient))
			, PlaybackState(MakeShared<FSharedPlaybackState>())
			, SequenceInstance(PlaybackState.ToSharedRef())
			, SubtitleData({ Section })
			, EvaluateTask(SequenceInstance.GetSharedPlaybackState()->GetLinker())
		{
			Subtitle->Duration = SubtitleDurationSeconds;
			Section->SetRange(TRange<FFrameNumber>(1, SubtitleDurationFrames));
			Subsystem->BindDelegates();
			Section->SetSubtitle(*Subtitle);

			// Reset TSharedRef FPlaybackState to avoid tripping an ensure in FSequenceInstance
			// Necessary because the tests are only using as little of the MovieScene system as possible
			PlaybackState.Reset();
		}

		UPROPERTY(Transient)
		UWorld* World = nullptr;

		UPROPERTY(Transient)
		USubtitlesSubsystem* Subsystem = nullptr;

		UPROPERTY(Transient)
		USubtitleAssetUserData* Subtitle = nullptr;

		UPROPERTY(Transient)
		UMovieSceneSubtitleSection* Section = nullptr;

		UPROPERTY(Transient)
		TSharedPtr<FSharedPlaybackState> PlaybackState;

		FSequenceInstance SequenceInstance;
		FSubtitleDataComponent SubtitleData;
		EMovieScenePlayerStatus::Type LastStatus;
		FEvaluateSubtitles EvaluateTask;
	};

	// Verify base case that a subtitle can be queued (and therefore displayed) by the subtitle subsystem
	// Ensures basic functionality that subtitles can actually be added to the subsystem.
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(UMovieSceneSubtitlesSystem_EvaluateForwardsQueuesSubtitle, "Subtitles.MovieSceneSubtitlesSystem.EvaluateForwardsQueuesSubtitle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		bool UMovieSceneSubtitlesSystem_EvaluateForwardsQueuesSubtitle::RunTest(const FString&)
	{
		FMovieSceneSubtitlesTest Test;

		// Evaluate a playing frame.
		const EMovieScenePlayerStatus::Type CurrentStatus = EMovieScenePlayerStatus::Type::Playing;
		Test.SequenceInstance.SetContext(FMovieSceneContext(FMovieSceneEvaluationRange(FFrameTime(1), TestFrameRate), CurrentStatus));
		Test.EvaluateTask.Evaluate(Test.SequenceInstance, Test.SubtitleData, Test.LastStatus);

		const FActiveSubtitle* FoundSubtitle = Test.Subsystem->GetActiveSubtitles().FindByPredicate(
			[Subtitle = Test.Subtitle](const FActiveSubtitle& ActiveSubtitle) {return Subtitle == ActiveSubtitle.Subtitle; });

		UTEST_NOT_NULL("Subtitle is active", FoundSubtitle);

		return true;
	}

	// Verify that all subtitles can be stopped by the subtitle movie scene system
	// Ensures that when playing/stopping/scrubbing in editor that any active subtitles can be reset on movie scene status change.
	// e.g. When scrubbing through sequencer during evaluation subtitles can be enqueued. Stop all of these and start fresh when playing forwards.
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(UMovieSceneSubtitlesSystem_EvaluateHasStartedStopsAllSubtitles, "Subtitles.MovieSceneSubtitlesSystem.EvaluateHasStartedStopsAllSubtitles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		bool UMovieSceneSubtitlesSystem_EvaluateHasStartedStopsAllSubtitles::RunTest(const FString&)
	{
		FMovieSceneSubtitlesTest Test;
		const USubtitleAssetUserData* Subtitle1 = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);
		const USubtitleAssetUserData* Subtitle2 = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);
		const USubtitleAssetUserData* Subtitle3 = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);

		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle1 });
		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle2 });
		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle3 });
		UTEST_EQUAL("All subtitles active", Test.Subsystem->GetActiveSubtitles().Num(), 3);

		Test.LastStatus = EMovieScenePlayerStatus::Type::Stopped;
		EMovieScenePlayerStatus::Type CurrentStatus = EMovieScenePlayerStatus::Type::Playing;
		Test.SequenceInstance.SetContext(FMovieSceneContext(FMovieSceneEvaluationRange(FFrameTime(0), TestFrameRate), CurrentStatus));
		Test.EvaluateTask.Evaluate(Test.SequenceInstance, Test.SubtitleData, Test.LastStatus);

		const FActiveSubtitle* FoundSubtitle = Test.Subsystem->GetActiveSubtitles().FindByPredicate(
			[Subtitle = Test.Subtitle](const FActiveSubtitle& ActiveSubtitle) {return Subtitle == ActiveSubtitle.Subtitle; });

		UTEST_NOT_NULL("All subtitles removed before queueing new subtitle", FoundSubtitle);

		return true;
	}

	// Verify that all subtitles can be displayed indefinitely
	// Ensures that when you pause on an active subtitle that its duration does not expire
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(UMovieSceneSubtitlesSystem_EvaluateStoppedQueuesInfiniteDuration, "Subtitles.MovieSceneSubtitlesSystem.EvaluateStoppedQueuesInfiniteDuration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		bool UMovieSceneSubtitlesSystem_EvaluateStoppedQueuesInfiniteDuration::RunTest(const FString&)
	{
		FMovieSceneSubtitlesTest Test;
		Test.LastStatus = EMovieScenePlayerStatus::Type::Playing;
		EMovieScenePlayerStatus::Type CurrentStatus = EMovieScenePlayerStatus::Type::Stopped;
		Test.SequenceInstance.SetContext(FMovieSceneContext(FMovieSceneEvaluationRange(FFrameTime(30), TestFrameRate), CurrentStatus));

		Test.EvaluateTask.Evaluate(Test.SequenceInstance, Test.SubtitleData, Test.LastStatus);

		UTEST_TRUE("A Subtitle is active", !Test.Subsystem->GetActiveSubtitles().IsEmpty());

		const float ExpectedDuration = FLT_MAX;
		const float ActualDuration = Test.World->GetTimerManager().GetTimerRemaining(Test.Subsystem->GetActiveSubtitles()[0].DurationTimerHandle);
		UTEST_EQUAL_TOLERANCE("Subtitle played mid-way is queued with remaining duration", ActualDuration, ExpectedDuration, FLT_EPSILON);

		return true;
	}

	// Verify that the subtitle section only queues its subtitle once
	// Ensures that the subtitle section doesn't spam the subtitle subsystem
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(UMovieSceneSubtitlesSystem_EvaluateDontQueueIfSubtitleAlreadyActive, "Subtitles.MovieSceneSubtitlesSystem.EvaluateDontQueueIfSubtitleAlreadyActive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		bool UMovieSceneSubtitlesSystem_EvaluateDontQueueIfSubtitleAlreadyActive::RunTest(const FString&)
	{
		FMovieSceneSubtitlesTest Test;
		Test.Subtitle->Duration = 1.f;
		EMovieScenePlayerStatus::Type CurrentStatus = EMovieScenePlayerStatus::Type::Playing;

		Test.SequenceInstance.SetContext(FMovieSceneContext(FMovieSceneEvaluationRange(FFrameTime(0), TestFrameRate), CurrentStatus));
		Test.EvaluateTask.Evaluate(Test.SequenceInstance, Test.SubtitleData, Test.LastStatus);

		Test.SequenceInstance.SetContext(FMovieSceneContext(FMovieSceneEvaluationRange(FFrameTime(1), TestFrameRate), CurrentStatus));
		Test.EvaluateTask.Evaluate(Test.SequenceInstance, Test.SubtitleData, Test.LastStatus);

		Test.SequenceInstance.SetContext(FMovieSceneContext(FMovieSceneEvaluationRange(FFrameTime(2), TestFrameRate), CurrentStatus));
		Test.EvaluateTask.Evaluate(Test.SequenceInstance, Test.SubtitleData, Test.LastStatus);

		Test.SequenceInstance.SetContext(FMovieSceneContext(FMovieSceneEvaluationRange(FFrameTime(3), TestFrameRate), CurrentStatus));
		Test.EvaluateTask.Evaluate(Test.SequenceInstance, Test.SubtitleData, Test.LastStatus);

		UTEST_EQUAL("SubtitleSection's Subtitle is only queued once", Test.Subsystem->GetActiveSubtitles().Num(), 1);

		return true;
	}

} // namespace SubtitlesAndClosedCaptions::Test

#endif
#endif