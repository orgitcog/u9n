// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencedImageTrackInfo.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::MetaHuman::Private
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTracksHaveDifferentFrameRatesTest_Empty,
	"MetaHuman.Capture.CaptureData.TracksHaveDifferentFrameRates.Empty",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTracksHaveDifferentFrameRatesTest_Empty::RunTest(const FString& InParameters)
{
	UTEST_FALSE("Tracks have different frame rates", TracksHaveDifferentFrameRates({}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTracksHaveDifferentFrameRatesTest_EqualFrameRates,
	"MetaHuman.Capture.CaptureData.TracksHaveDifferentFrameRates.EqualFrameRates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTracksHaveDifferentFrameRatesTest_EqualFrameRates::RunTest(const FString& InParameters)
{
	const TArray<TPair<FFrameRate, FFrameRate>> EqualRates = {
		{FFrameRate(24'000, 1'000), FFrameRate(24'000, 1'000)},
		{FFrameRate(25'000, 1'000), FFrameRate(25'000, 1'000)},
		{FFrameRate(30'000, 1'000), FFrameRate(30'000, 1'000)},
		{FFrameRate(30'000, 1'001), FFrameRate(30'000, 1'001)},
		{FFrameRate(48'000, 1'000), FFrameRate(48'000, 1'000)},
		{FFrameRate(50'000, 1'000), FFrameRate(50'000, 1'000)},
		{FFrameRate(60'000, 1'000), FFrameRate(60'000, 1'000)},
		{FFrameRate(60'000, 1'001), FFrameRate(60'000, 1'001)},
	};

	for (const TPair<FFrameRate, FFrameRate>& RatePair : EqualRates)
	{
		{
			TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
			SequencedImageTrackInfos.Emplace(RatePair.Key, TRange<FFrameNumber>(1, 1));
			SequencedImageTrackInfos.Emplace(RatePair.Value, TRange<FFrameNumber>(1, 1));

			UTEST_FALSE(
				FString::Printf(TEXT("Tracks are compatible (%.2f vs %.2f)"), RatePair.Key.AsDecimal(), RatePair.Value.AsDecimal()),
				TracksHaveDifferentFrameRates(SequencedImageTrackInfos)
			);
		}

		// Reverse the arguments
		{
			TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
			SequencedImageTrackInfos.Emplace(RatePair.Value, TRange<FFrameNumber>(1, 1));
			SequencedImageTrackInfos.Emplace(RatePair.Key, TRange<FFrameNumber>(1, 1));

			UTEST_FALSE(
				FString::Printf(TEXT("Tracks are compatible (%.2f vs %.2f)"), RatePair.Value.AsDecimal(), RatePair.Key.AsDecimal()),
				TracksHaveDifferentFrameRates(SequencedImageTrackInfos)
			);
		}

	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTracksHaveDifferentFrameRatesTest_DifferentFrameRates,
	"MetaHuman.Capture.CaptureData.TracksHaveDifferentFrameRates.DifferentFrameRates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTracksHaveDifferentFrameRatesTest_DifferentFrameRates::RunTest(const FString& InParameters)
{
	const TArray<TPair<FFrameRate, FFrameRate>> DifferentRates = {
		{FFrameRate(24'000, 1'000), FFrameRate(24'000, 1'001)},
		{FFrameRate(25'000, 1'000), FFrameRate(25'000, 1'001)},
		{FFrameRate(30'000, 1'000), FFrameRate(30'000, 1'001)},
		{FFrameRate(48'000, 1'000), FFrameRate(48'000, 1'001)},
		{FFrameRate(50'000, 1'000), FFrameRate(50'000, 1'001)},
		{FFrameRate(60'000, 1'000), FFrameRate(60'000, 1'001)},
	};

	for (const TPair<FFrameRate, FFrameRate>& RatePair : DifferentRates)
	{
		{
			TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
			SequencedImageTrackInfos.Emplace(RatePair.Key, TRange<FFrameNumber>(1, 1));
			SequencedImageTrackInfos.Emplace(RatePair.Value, TRange<FFrameNumber>(1, 1));

			UTEST_TRUE(
				FString::Printf(TEXT("Tracks are compatible (%.2f vs %.2f)"), RatePair.Key.AsDecimal(), RatePair.Value.AsDecimal()),
				TracksHaveDifferentFrameRates(SequencedImageTrackInfos)
			);
		}

		// Reverse the arguments
		{
			TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
			SequencedImageTrackInfos.Emplace(RatePair.Value, TRange<FFrameNumber>(1, 1));
			SequencedImageTrackInfos.Emplace(RatePair.Key, TRange<FFrameNumber>(1, 1));

			UTEST_TRUE(
				FString::Printf(TEXT("Tracks are compatible (%.2f vs %.2f)"), RatePair.Value.AsDecimal(), RatePair.Key.AsDecimal()),
				TracksHaveDifferentFrameRates(SequencedImageTrackInfos)
			);
		}
	}

	return true;
}

}

#endif // WITH_DEV_AUTOMATION_TESTS