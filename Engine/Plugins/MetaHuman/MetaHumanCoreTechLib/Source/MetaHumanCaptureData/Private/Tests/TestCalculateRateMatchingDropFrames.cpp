// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencedImageTrackInfo.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::MetaHuman::Private
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCalculateRateMatchingDropFramesTest_Empty,
	"MetaHuman.Capture.CaptureData.CalculateRateMatchingDropFrames.Empty",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FCalculateRateMatchingDropFramesTest_Empty::RunTest(const FString& InParameters)
{
	const FFrameRate TargetFrameRate(60'000, 1'000);
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;

	const TArray<FFrameNumber> RateMatchingDropFrames = CalculateRateMatchingDropFrames(TargetFrameRate, SequencedImageTrackInfos);
	const TArray<FFrameNumber> ExpectedRateMatchingDropFrames;
	UTEST_EQUAL("Drop frames", RateMatchingDropFrames, ExpectedRateMatchingDropFrames);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCalculateRateMatchingDropFramesTest_SingleEntry,
	"MetaHuman.Capture.CaptureData.CalculateRateMatchingDropFrames.SingleEntry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FCalculateRateMatchingDropFramesTest_SingleEntry::RunTest(const FString& InParameters)
{
	const FFrameRate TargetFrameRate(60'000, 1'000);
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(0, 10)
		},
	};

	const TRange<FFrameNumber> RangeLimit(0, 10);
	const TArray<FFrameNumber> RateMatchingDropFrames = CalculateRateMatchingDropFrames(TargetFrameRate, SequencedImageTrackInfos, RangeLimit);
	UTEST_TRUE("Drop frames", RateMatchingDropFrames.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCalculateRateMatchingDropFramesTest_TargetRateDoubleLowestRate,
	"MetaHuman.Capture.CaptureData.CalculateRateMatchingDropFrames.TargetRateDoubleLowestRate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FCalculateRateMatchingDropFramesTest_TargetRateDoubleLowestRate::RunTest(const FString& InParameters)
{
	const FFrameRate TargetFrameRate(60'000, 1'000);
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(0, 20)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(0, 10)
		},
	};

	const TRange<FFrameNumber> RangeLimit(0, 20);
	const TArray<FFrameNumber> RateMatchingDropFrames = CalculateRateMatchingDropFrames(TargetFrameRate, SequencedImageTrackInfos, RangeLimit);

	// We expect to "drop" every second frame, as the 30 fps track is missing frames for these target frame numbers.
	const TArray<FFrameNumber> ExpectedRateMatchingDropFrames = { 1, 3, 5, 7, 9, 11, 13, 15, 17, 19 };

	UTEST_EQUAL("Drop frames count", RateMatchingDropFrames.Num(), ExpectedRateMatchingDropFrames.Num());

	for (int32 Idx = 0; Idx < RateMatchingDropFrames.Num(); ++Idx)
	{
		const FFrameNumber RateMatchingDropFrame = RateMatchingDropFrames[Idx];
		const FFrameNumber ExpectedRateMatchingDropFrame = ExpectedRateMatchingDropFrames[Idx];

		UTEST_EQUAL(
			FString::Format(
				TEXT("Frame at index {0} (Value={1}) should match the expected value at index {0} (Value={3})"),
				{ Idx, RateMatchingDropFrame.Value, ExpectedRateMatchingDropFrame.Value }
			),
			RateMatchingDropFrame.Value,
			ExpectedRateMatchingDropFrame.Value
		);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCalculateRateMatchingDropFramesTest_TargetRateHalfHighestRate,
	"MetaHuman.Capture.CaptureData.CalculateRateMatchingDropFrames.TargetRateHalfHighestRate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FCalculateRateMatchingDropFramesTest_TargetRateHalfHighestRate::RunTest(const FString& InParameters)
{
	const FFrameRate TargetFrameRate(30'000, 1'000);
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(0, 20)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(0, 10)
		},
	};

	const TRange<FFrameNumber> RangeLimit(0, 10);
	const TArray<FFrameNumber> RateMatchingDropFrames = CalculateRateMatchingDropFrames(TargetFrameRate, SequencedImageTrackInfos, RangeLimit);

	// The 60fps track will just take every second frame (there are no "missing" frames), so no need to drop in this case
	UTEST_TRUE("Drop frames is empty", RateMatchingDropFrames.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCalculateRateMatchingDropFramesTest_TargetRateDoubleLowestRateWithNonNonZeroStart,
	"MetaHuman.Capture.CaptureData.CalculateRateMatchingDropFrames.TargetRateDoubleLowestRateWithNonNonZeroStart",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FCalculateRateMatchingDropFramesTest_TargetRateDoubleLowestRateWithNonNonZeroStart::RunTest(const FString& InParameters)
{
	const FFrameRate TargetFrameRate(60'000, 1'000);
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(5, 18)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(7, 16)
		},
	};

	const TRange<FFrameNumber> RangeLimit(7, 16);
	const TArray<FFrameNumber> RateMatchingDropFrames = CalculateRateMatchingDropFrames(TargetFrameRate, SequencedImageTrackInfos, RangeLimit);
	const TArray<FFrameNumber> ExpectedRateMatchingDropFrames = { 8, 10, 12, 14, 16 };

	UTEST_EQUAL("Drop frames count", RateMatchingDropFrames.Num(), ExpectedRateMatchingDropFrames.Num());

	for (int32 Idx = 0; Idx < RateMatchingDropFrames.Num(); ++Idx)
	{
		const FFrameNumber RateMatchingDropFrame = RateMatchingDropFrames[Idx];
		const FFrameNumber ExpectedRateMatchingDropFrame = ExpectedRateMatchingDropFrames[Idx];

		UTEST_EQUAL(
			FString::Format(
				TEXT("Frame at index {0} (Value={1}) should match the expected value at index {0} (Value={3})"),
				{ Idx, RateMatchingDropFrame.Value, ExpectedRateMatchingDropFrame.Value }
			),
			RateMatchingDropFrame.Value,
			ExpectedRateMatchingDropFrame.Value
		);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCalculateRateMatchingDropFramesTest_DropUntilFirstCommonFrame,
	"MetaHuman.Capture.CaptureData.CalculateRateMatchingDropFrames.DropUntilFirstCommonFrame",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FCalculateRateMatchingDropFramesTest_DropUntilFirstCommonFrame::RunTest(const FString& InParameters)
{
	const FFrameRate TargetFrameRate(60'000, 1'000);
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(9, 18)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(7, 16)
		},
	};

	const TArray<FFrameNumber> RateMatchingDropFrames = CalculateRateMatchingDropFrames(TargetFrameRate, SequencedImageTrackInfos);
	// Drop until frames appear in both tracks and then start the normal drop procedure
	const TArray<FFrameNumber> ExpectedRateMatchingDropFrames = { 7, 8, 10, 12, 14, 16, 18 };

	UTEST_EQUAL("Drop frames count", RateMatchingDropFrames.Num(), ExpectedRateMatchingDropFrames.Num());

	for (int32 Idx = 0; Idx < RateMatchingDropFrames.Num(); ++Idx)
	{
		const FFrameNumber RateMatchingDropFrame = RateMatchingDropFrames[Idx];
		const FFrameNumber ExpectedRateMatchingDropFrame = ExpectedRateMatchingDropFrames[Idx];

		UTEST_EQUAL(
			FString::Format(
				TEXT("Frame at index {0} (Value={1}) should match the expected value at index {0} (Value={3})"),
				{ Idx, RateMatchingDropFrame.Value, ExpectedRateMatchingDropFrame.Value }
			),
			RateMatchingDropFrame.Value,
			ExpectedRateMatchingDropFrame.Value
		);
	}

	return true;
}

}

#endif // WITH_DEV_AUTOMATION_TESTS
