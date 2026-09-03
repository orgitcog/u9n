// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencedImageTrackInfo.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::MetaHuman::Private
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameRatesAreCompatibleTest_EqualRates,
	"MetaHuman.Capture.CaptureData.FrameRatesAreCompatible.EqualRates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameRatesAreCompatibleTest_EqualRates::RunTest(const FString& InParameters)
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
		UTEST_TRUE(
			FString::Printf(TEXT("Frame rates compatible (%.2f vs %.2f)"), RatePair.Key.AsDecimal(), RatePair.Value.AsDecimal()),
			FrameRatesAreCompatible(RatePair.Key, RatePair.Value)
		);

		// Reverse the arguments
		UTEST_TRUE(
			FString::Printf(TEXT("Frame rates compatible (%.2f vs %.2f)"), RatePair.Value.AsDecimal(), RatePair.Key.AsDecimal()),
			FrameRatesAreCompatible(RatePair.Value, RatePair.Key)
		);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameRatesAreCompatibleTest_CompatibleRates,
	"MetaHuman.Capture.CaptureData.FrameRatesAreCompatible.CompatibleRates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameRatesAreCompatibleTest_CompatibleRates::RunTest(const FString& InParameters)
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
		UTEST_TRUE(
			FString::Printf(TEXT("Frame rates compatible (%.2f vs %.2f)"), RatePair.Key.AsDecimal(), RatePair.Value.AsDecimal()),
			FrameRatesAreCompatible(RatePair.Key, RatePair.Value)
		);

		// Reverse the arguments
		UTEST_TRUE(
			FString::Printf(TEXT("Frame rates compatible (%.2f vs %.2f)"), RatePair.Value.AsDecimal(), RatePair.Key.AsDecimal()),
			FrameRatesAreCompatible(RatePair.Value, RatePair.Key)
		);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameRatesAreCompatibleTest_IncompatibleRates,
	"MetaHuman.Capture.CaptureData.FrameRatesAreCompatible.IncompatibleRates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameRatesAreCompatibleTest_IncompatibleRates::RunTest(const FString& InParameters)
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
		UTEST_FALSE(
			FString::Printf(TEXT("Frame rates compatible (%.2f vs %.2f)"), RatePair.Key.AsDecimal(), RatePair.Value.AsDecimal()),
			FrameRatesAreCompatible(RatePair.Key, RatePair.Value)
		);

		// Reverse the arguments
		UTEST_FALSE(
			FString::Printf(TEXT("Frame rates compatible (%.2f vs %.2f)"), RatePair.Value.AsDecimal(), RatePair.Key.AsDecimal()),
			FrameRatesAreCompatible(RatePair.Value, RatePair.Key)
		);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameRatesAreCompatibleTest_ZeroFrameRate,
	"MetaHuman.Capture.CaptureData.FrameRatesAreCompatible.ZeroFrameRate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameRatesAreCompatibleTest_ZeroFrameRate::RunTest(const FString& InParameters)
{
	const FFrameRate FirstFrameRate(60'000, 1'000);
	const FFrameRate SecondFrameRate(0, 1'000);

	// Check that we're avoiding a divide by zero problem

	UTEST_FALSE("Frame rates are compatible", FrameRatesAreCompatible(FirstFrameRate, SecondFrameRate));

	// Reverse the arguments
	UTEST_FALSE("Frame rates are compatible", FrameRatesAreCompatible(SecondFrameRate, FirstFrameRate));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameRatesAreCompatibleTest_TwoZeroFrameRates,
	"MetaHuman.Capture.CaptureData.FrameRatesAreCompatible.TwoZeroFrameRates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameRatesAreCompatibleTest_TwoZeroFrameRates::RunTest(const FString& InParameters)
{
	const FFrameRate FirstFrameRate(0, 1'000);
	const FFrameRate SecondFrameRate(0, 1'000);

	// We treat these as compatible despite the peculiarity of using zero frame rates, they are equal

	UTEST_TRUE("Frame rates are compatible", FrameRatesAreCompatible(FirstFrameRate, SecondFrameRate));

	// Reverse the arguments
	UTEST_TRUE("Frame rates are compatible", FrameRatesAreCompatible(SecondFrameRate, FirstFrameRate));

	return true;
}


}

#endif // WITH_DEV_AUTOMATION_TESTS