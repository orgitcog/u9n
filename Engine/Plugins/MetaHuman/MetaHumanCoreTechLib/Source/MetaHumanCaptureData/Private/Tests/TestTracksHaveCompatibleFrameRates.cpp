// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencedImageTrackInfo.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::MetaHuman::Private
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTracksHaveCompatibleFrameRates_EmptyTest,
	"MetaHuman.Capture.CaptureData.TracksHaveCompatibleFrameRates.Empty",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTracksHaveCompatibleFrameRates_EmptyTest::RunTest(const FString& InParameters)
{
	UTEST_TRUE("Tracks are compatible", TracksHaveCompatibleFrameRates({}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTracksHaveCompatibleFrameRates_SingleEntryTest,
	"MetaHuman.Capture.CaptureData.TracksHaveCompatibleFrameRates.SingleEntry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTracksHaveCompatibleFrameRates_SingleEntryTest::RunTest(const FString& InParameters)
{
	TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
	SequencedImageTrackInfos.Emplace(FFrameRate(60'000, 1'000), TRange<FFrameNumber>(1, 1));

	UTEST_TRUE("Tracks are compatible", TracksHaveCompatibleFrameRates(SequencedImageTrackInfos));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTracksHaveCompatibleFrameRates_EqualRatesTest,
	"MetaHuman.Capture.CaptureData.TracksHaveCompatibleFrameRates.EqualRates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTracksHaveCompatibleFrameRates_EqualRatesTest::RunTest(const FString& InParameters)
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

			UTEST_TRUE(
				FString::Printf(TEXT("Tracks are compatible (%.2f vs %.2f)"), RatePair.Key.AsDecimal(), RatePair.Value.AsDecimal()),
				TracksHaveCompatibleFrameRates(SequencedImageTrackInfos)
			);
		}

		// Reverse the arguments
		{
			TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
			SequencedImageTrackInfos.Emplace(RatePair.Value, TRange<FFrameNumber>(1, 1));
			SequencedImageTrackInfos.Emplace(RatePair.Key, TRange<FFrameNumber>(1, 1));

			UTEST_TRUE(
				FString::Printf(TEXT("Tracks are compatible (%.2f vs %.2f)"), RatePair.Value.AsDecimal(), RatePair.Key.AsDecimal()),
				TracksHaveCompatibleFrameRates(SequencedImageTrackInfos)
			);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTracksHaveCompatibleFrameRates_CompatibleRatesTest,
	"MetaHuman.Capture.CaptureData.TracksHaveCompatibleFrameRates.CompatibleRates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTracksHaveCompatibleFrameRates_CompatibleRatesTest::RunTest(const FString& InParameters)
{
	const TArray<TPair<FFrameRate, FFrameRate>> CompatibleRates = {
		{FFrameRate(24'000, 1'000), FFrameRate(48'000, 1'000)},
		{FFrameRate(25'000, 1'000), FFrameRate(50'000, 1'000)},
		{FFrameRate(30'000, 1'000), FFrameRate(60'000, 1'000)},
		{FFrameRate(30'000, 1'001), FFrameRate(60'000, 1'001)},
		{FFrameRate(60'000, 1'000), FFrameRate(120'000, 1'000)},
	};

	for (const TPair<FFrameRate, FFrameRate>& RatePair : CompatibleRates)
	{
		{
			TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
			SequencedImageTrackInfos.Emplace(RatePair.Key, TRange<FFrameNumber>(1, 1));
			SequencedImageTrackInfos.Emplace(RatePair.Value, TRange<FFrameNumber>(1, 1));

			UTEST_TRUE(
				FString::Printf(TEXT("Tracks are compatible (%.2f vs %.2f)"), RatePair.Key.AsDecimal(), RatePair.Value.AsDecimal()),
				TracksHaveCompatibleFrameRates(SequencedImageTrackInfos)
			);
		}

		// Reverse the arguments
		{
			TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
			SequencedImageTrackInfos.Emplace(RatePair.Value, TRange<FFrameNumber>(1, 1));
			SequencedImageTrackInfos.Emplace(RatePair.Key, TRange<FFrameNumber>(1, 1));

			UTEST_TRUE(
				FString::Printf(TEXT("Tracks are compatible (%.2f vs %.2f)"), RatePair.Value.AsDecimal(), RatePair.Key.AsDecimal()),
				TracksHaveCompatibleFrameRates(SequencedImageTrackInfos)
			);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTracksHaveCompatibleFrameRates_IncompatibleRatesTest,
	"MetaHuman.Capture.CaptureData.TracksHaveCompatibleFrameRates.IncompatibleRates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTracksHaveCompatibleFrameRates_IncompatibleRatesTest::RunTest(const FString& InParameters)
{
	const TArray<TPair<FFrameRate, FFrameRate>> IncompatibleRates = {
		{FFrameRate(24'000, 1'000), FFrameRate(30'000, 1'001)},
		{FFrameRate(24'000, 1'000), FFrameRate(30'000, 1'000)},
		{FFrameRate(24'000, 1'000), FFrameRate(50'000, 1'000)},
		{FFrameRate(24'000, 1'000), FFrameRate(60'000, 1'000)},
		{FFrameRate(24'000, 1'000), FFrameRate(60'000, 1'001)},

		{FFrameRate(25'000, 1'000), FFrameRate(30'000, 1'000)},
		{FFrameRate(25'000, 1'000), FFrameRate(48'000, 1'000)},
		{FFrameRate(25'000, 1'000), FFrameRate(60'000, 1'000)},
		{FFrameRate(25'000, 1'000), FFrameRate(60'000, 1'001)},

		{FFrameRate(30'000, 1'000), FFrameRate(30'000, 1'001)},
		{FFrameRate(30'000, 1'000), FFrameRate(48'000, 1'000)},
		{FFrameRate(30'000, 1'000), FFrameRate(50'000, 1'000)},
		{FFrameRate(30'000, 1'000), FFrameRate(60'000, 1'001)},

		{FFrameRate(48'000, 1'000), FFrameRate(30'000, 1'001)},
		{FFrameRate(48'000, 1'000), FFrameRate(50'000, 1'000)},
		{FFrameRate(48'000, 1'000), FFrameRate(60'000, 1'000)},
		{FFrameRate(48'000, 1'000), FFrameRate(60'000, 1'001)},

		{FFrameRate(50'000, 1'000), FFrameRate(30'000, 1'001)},
		{FFrameRate(50'000, 1'000), FFrameRate(60'000, 1'000)},
		{FFrameRate(50'000, 1'000), FFrameRate(60'000, 1'001)},

		{FFrameRate(60'000, 1'000), FFrameRate(30'000, 1'001)},
		{FFrameRate(60'000, 1'000), FFrameRate(60'000, 1'001)},
	};

	for (const TPair<FFrameRate, FFrameRate>& RatePair : IncompatibleRates)
	{
		{
			TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
			SequencedImageTrackInfos.Emplace(RatePair.Key, TRange<FFrameNumber>(1, 1));
			SequencedImageTrackInfos.Emplace(RatePair.Value, TRange<FFrameNumber>(1, 1));

			UTEST_FALSE(
				FString::Printf(TEXT("Tracks are compatible (%.2f vs %.2f)"), RatePair.Key.AsDecimal(), RatePair.Value.AsDecimal()),
				TracksHaveCompatibleFrameRates(SequencedImageTrackInfos)
			);
		}

		// Reverse the arguments
		{
			TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
			SequencedImageTrackInfos.Emplace(RatePair.Value, TRange<FFrameNumber>(1, 1));
			SequencedImageTrackInfos.Emplace(RatePair.Key, TRange<FFrameNumber>(1, 1));

			UTEST_FALSE(
				FString::Printf(TEXT("Tracks are compatible (%.2f vs %.2f)"), RatePair.Value.AsDecimal(), RatePair.Key.AsDecimal()),
				TracksHaveCompatibleFrameRates(SequencedImageTrackInfos)
			);
		}
	}

	return true;
}
}


#endif // WITH_DEV_AUTOMATION_TESTS