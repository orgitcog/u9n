// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"
#include "SubtitlesSubsystem.h"
#include "TimerManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SubtitlesAndClosedCaptions::Test
{
	// Helper struct for initializing tests so the same thing doesn't have to be done in every class
	struct FSubtitlesTest
	{
		FSubtitlesTest()
			: World(NewObject<UWorld>(GetTransientPackage(), NAME_None, RF_Transient))
			, Subsystem(NewObject<USubtitlesSubsystem>(World, NAME_None, RF_Transient))
		{
			Subsystem->BindDelegates();
		}

		UWorld* World = nullptr;
		USubtitlesSubsystem* Subsystem = nullptr;
	};

	// Verify that a subtitle can be added to the subtitle subsystem.
	// Ensures basic functionality that subtitles can actually be added to the subsystem.
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(USubtitlesSubsystem_QueueSubtitle, "Subtitles.SubtitlesSubsystem.QueueSubtitle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		bool USubtitlesSubsystem_QueueSubtitle::RunTest(const FString&)
	{
		FSubtitlesTest Test;
		const USubtitleAssetUserData* Subtitle = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);

		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle });
		UTEST_EQUAL("Active subtitle successfully added", Test.Subsystem->GetActiveSubtitles().Num(), 1);

		return true;
	}

	// Verify that a subtitle can be stopped manually before elapsing its entire duration
	// Ensures that gameplay or any systems have the ability to cut off a subtitle
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(USubtitlesSubsystem_StopSubtitle, "Subtitles.SubtitlesSubsystem.StopSubtitle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		bool USubtitlesSubsystem_StopSubtitle::RunTest(const FString&)
	{
		FSubtitlesTest Test;
		const USubtitleAssetUserData* Subtitle1 = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);
		const USubtitleAssetUserData* Subtitle2 = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);

		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle1 });
		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle2 });
		UTEST_TRUE("A subtitle is active", !Test.Subsystem->GetActiveSubtitles().IsEmpty());

		Test.Subsystem->StopSubtitle(*Subtitle1);
		const FActiveSubtitle* FoundSubtitle1 = Test.Subsystem->GetActiveSubtitles().FindByPredicate([Subtitle1](const FActiveSubtitle& ActiveSubtitle) { return ActiveSubtitle.Subtitle == Subtitle1; });
		UTEST_NULL("Subtitle1 was stopped", FoundSubtitle1);

		const FActiveSubtitle* FoundSubtitle2 = Test.Subsystem->GetActiveSubtitles().FindByPredicate([Subtitle2](const FActiveSubtitle& ActiveSubtitle) { return ActiveSubtitle.Subtitle == Subtitle2; });
		UTEST_NOT_NULL("Subtitle2 is still active", FoundSubtitle2);

		return true;
	}

	// Verify that subtitles are prioritized by descending order of priority
	// Ensures that when there are many subtitles trying to play at once (and being added/removed) that the prioritization sort order is maintained.
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(USubtitlesSubsystem_PrioritizeSubtitles, "Subtitles.SubtitlesSubsystem.PrioritizeSubtitles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		bool USubtitlesSubsystem_PrioritizeSubtitles::RunTest(const FString&)
	{
		FSubtitlesTest Test;

		// prioritized subtitles with priorities from 0 to 3, where 0 is the lowest priority and 3 is the highest priority
		// 0, 1, 2, 3
		constexpr uint32 NumSubtitles = 4;
		TStaticArray< const USubtitleAssetUserData*, NumSubtitles> Subtitles;
		for (int i = 0; i < NumSubtitles; ++i)
		{
			USubtitleAssetUserData* Subtitle = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);
			Subtitle->Priority = static_cast<float>(i);
			Subtitles[i] = Subtitle;
			Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle });
		}

		UTEST_TRUE("Add subtitles added", Test.Subsystem->GetActiveSubtitles().Num() == NumSubtitles);
		UTEST_TRUE("Highest priority subtitle (3) is first index", Test.Subsystem->GetActiveSubtitles()[0].Subtitle == Subtitles[3]);

		Test.Subsystem->StopSubtitle(*Subtitles[3]);
		UTEST_TRUE("After removing highest priority subtitle the new Highest priority subtitle is 2", Test.Subsystem->GetActiveSubtitles()[0].Subtitle == Subtitles[2]);

		Test.Subsystem->StopSubtitle(*Subtitles[0]);
		UTEST_TRUE("After removing lower priority subtitle the highest priority is still the same", Test.Subsystem->GetActiveSubtitles()[0].Subtitle == Subtitles[2]);

		return true;
	}

	// Verify that subtitle API supports displaying a subtitle for a specified duration, regardless of the duration on the asset.
	// Ensures that a subtitle can start half way through its duration, or play infinitely and be manually stopped 
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(USubtitlesSubsystem_OverrideDuration, "Subtitles.SubtitlesSubsystem.OverrideDuration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		bool USubtitlesSubsystem_OverrideDuration::RunTest(const FString&)
	{
		FSubtitlesTest Test;
		USubtitleAssetUserData* Subtitle = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);
		Subtitle->Duration = 0.f; // invalid duration as < SubtitleMinDuration

		constexpr float ExpectedDuration = 42.f;
		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle, ExpectedDuration });
		UTEST_TRUE("A subtitle is active", !Test.Subsystem->GetActiveSubtitles().IsEmpty());

		const float ActualDuration = Test.World->GetTimerManager().GetTimerRemaining(Test.Subsystem->GetActiveSubtitles()[0].DurationTimerHandle);
		UTEST_EQUAL_TOLERANCE("The expected subtitle has the expected duration", ActualDuration, ExpectedDuration, FLT_EPSILON);

		return true;
	}

	// Verify that subtitle API uses SubtitleMinDuration when a duration isn't set or otherwise is zero, regardless of the duration on the asset.
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(USubtitlesSubsystem_ClampDuration, "Subtitles.SubtitlesSubsystem.ClampDuration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool USubtitlesSubsystem_ClampDuration::RunTest(const FString&)
	{
		FSubtitlesTest Test;
		USubtitleAssetUserData* Subtitle = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);
		Subtitle->Duration = 0.f; // invalid duration as < SubtitleMinDuration

		constexpr float ExpectedDuration = SubtitleMinDuration;
		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle });
		UTEST_TRUE("A subtitle is active", !Test.Subsystem->GetActiveSubtitles().IsEmpty());

		const float ActualDuration = Test.World->GetTimerManager().GetTimerRemaining(Test.Subsystem->GetActiveSubtitles()[0].DurationTimerHandle);
		UTEST_EQUAL_TOLERANCE("The expected subtitle has the expected duration", ActualDuration, ExpectedDuration, FLT_EPSILON);

		return true;
	}

	// Verify that a user can check if the subtitle is already active
	// Ensures that systems like movie scene can check if a subtitle is already active before trying to queue one up every frame
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(USubtitlesSubsystem_IsSubtitleActive, "Subtitles.SubtitlesSubsystem.IsSubtitleActive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		bool USubtitlesSubsystem_IsSubtitleActive::RunTest(const FString&)
	{
		FSubtitlesTest Test;
		const USubtitleAssetUserData* Subtitle = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);

		UTEST_TRUE("Subtitle is not active", !Test.Subsystem->IsSubtitleActive(*Subtitle));

		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle });
		UTEST_TRUE("A subtitle is active", Test.Subsystem->IsSubtitleActive(*Subtitle));
		UTEST_SAME_PTR("The expected subtitle is active", Subtitle, Test.Subsystem->GetActiveSubtitles()[0].Subtitle.Get());

		return true;
	}

	// Verify that all subtitles can be stopped at once
	// Ensures that gameplay or any systems have the ability to stop all subtitles if for whatever reason there is a context or state that needs to reset subtitles
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(USubtitlesSubsystem_StopAllSubtitles, "Subtitles.SubtitlesSubsystem.StopAllSubtitles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		bool USubtitlesSubsystem_StopAllSubtitles::RunTest(const FString&)
	{
		FSubtitlesTest Test;
		const USubtitleAssetUserData* Subtitle1 = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);
		
		USubtitleAssetUserData* Subtitle2 = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);
		Subtitle2->StartOffset = 1.f;

		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle1 });
		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *Subtitle2 });
		UTEST_TRUE("A subtitle is active", !Test.Subsystem->GetActiveSubtitles().IsEmpty());

		Test.Subsystem->StopAllSubtitles();
		const FActiveSubtitle* FoundSubtitle1 = Test.Subsystem->GetActiveSubtitles().FindByPredicate([Subtitle1](const FActiveSubtitle& ActiveSubtitle) { return ActiveSubtitle.Subtitle == Subtitle1; });
		UTEST_NULL("Subtitle1 was stopped", FoundSubtitle1);

		const FActiveSubtitle* FoundSubtitle2 = Test.Subsystem->GetActiveSubtitles().FindByPredicate([Subtitle2](const FActiveSubtitle& ActiveSubtitle) { return ActiveSubtitle.Subtitle == Subtitle2; });
		UTEST_NULL("Subtitle2 was stopped", FoundSubtitle2);

		return true;
	}

	// Verify that delayed subtitles do not immediately join the queue, and that they do join the queue after their delay timer expires.
	// Ensures that migrated subtitles using a delayed start will continue to start at their expected times.
	// Test content is all transient and uses a new USubtitlesSubsystem that has no UWorld.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(USubtitlesSubsystem_DelayedOffset, "Subtitles.SubtitlesSubsystem.CheckDelayedOffsets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
		bool USubtitlesSubsystem_DelayedOffset::RunTest(const FString&)
	{
		FSubtitlesTest Test;
		USubtitleAssetUserData* InstantSubtitle = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);
		USubtitleAssetUserData* DelayedSubtitle = NewObject<USubtitleAssetUserData>(GetTransientPackage(), NAME_None, RF_Transient);

		// Both subtitles have the default duration of 3 seconds.
		DelayedSubtitle->StartOffset = 1.f;

		// The delayed subtitle should have priority when its delay timer expires.
		DelayedSubtitle->Priority = 999;
		InstantSubtitle->Priority = 1;
		
		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *InstantSubtitle });
		Test.Subsystem->QueueSubtitle(FQueueSubtitleParameters{ *DelayedSubtitle });
		UTEST_TRUE("Only the instant subtitle is in the queue.", Test.Subsystem->GetActiveSubtitles().Num() == 1);

		Test.Subsystem->TestActivatingDelayedSubtitle(*DelayedSubtitle);
		UTEST_TRUE("There are two subtitles in the queue now.", Test.Subsystem->GetActiveSubtitles().Num() == 2);
		UTEST_SAME_PTR("The delayed subtitle is now active.", DelayedSubtitle, Test.Subsystem->GetTopRankedSubtitle());

		return true;
	}

} // namespace SubtitlesAndClosedCaptions::Test

#endif
