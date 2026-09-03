// Copyright Epic Games, Inc. All Rights Reserved.

#include "FrameNumberTransformer.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::MetaHuman
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameNumberTransformerTest_NoMapping,
	"ProductFilter.CaptureData.FrameNumberTransformer.NoTransform",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameNumberTransformerTest_NoMapping::RunTest(const FString& InParameters)
{
	FFrameNumberTransformer FrameNumberTransformer;

	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(0), 0);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(1), 1);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(2), 2);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(3), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameNumberTransformerTest_SimpleOffset,
	"ProductFilter.CaptureData.FrameNumberTransformer.SimpleOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameNumberTransformerTest_SimpleOffset::RunTest(const FString& InParameters)
{
	constexpr int32 FrameNumberOffset = 2;
	FFrameNumberTransformer FrameNumberTransformer(FrameNumberOffset);

	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(0), 2);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(1), 3);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(2), 4);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(3), 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameNumberTransformerTest_TargetRateHigher,
	"ProductFilter.CaptureData.FrameNumberTransformer.TargetRateHigher",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameNumberTransformerTest_TargetRateHigher::RunTest(const FString& InParameters)
{
	const FFrameRate SourceFrameRate(30'000, 1'000);
	const FFrameRate TargetFrameRate(60'000, 1'000);
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate);

	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(0), 0);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(1), 0);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(2), 1);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(3), 1);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(4), 2);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(5), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameNumberTransformerTest_TargetRateLower,
	"ProductFilter.CaptureData.FrameNumberTransformer.TargetRateLower",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameNumberTransformerTest_TargetRateLower::RunTest(const FString& InParameters)
{
	const FFrameRate SourceFrameRate(48'000, 1'000);
	const FFrameRate TargetFrameRate(24'000, 1'000);
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate);

	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(0), 0);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(1), 2);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(2), 4);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(3), 6);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(4), 8);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(5), 10);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameNumberTransformerTest_TargetRateLowerWithOffset,
	"ProductFilter.CaptureData.FrameNumberTransformer.TargetRateLowerWithOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameNumberTransformerTest_TargetRateLowerWithOffset::RunTest(const FString& InParameters)
{
	constexpr int32 FrameNumberOffset = 3;
	const FFrameRate SourceFrameRate(24'000, 1'000);
	const FFrameRate TargetFrameRate(12'000, 1'000);
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate, FrameNumberOffset);

	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(0), 6); // 0 -> 3 * 2
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(1), 8); // 1 -> 4 * 2
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(2), 10);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(3), 12);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(4), 14);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(5), 16);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameNumberTransformerTest_TargetRateHigherWithOffset,
	"ProductFilter.CaptureData.FrameNumberTransformer.TargetRateHigherWithOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameNumberTransformerTest_TargetRateHigherWithOffset::RunTest(const FString& InParameters)
{
	constexpr int32 FrameNumberOffset = 3;
	const FFrameRate SourceFrameRate(25'000, 1'000);
	const FFrameRate TargetFrameRate(50'000, 1'000);
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate, FrameNumberOffset);

	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(0), 1); // 0 -> 3 / 2 Floored
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(1), 2); // 1 -> 4 / 2 Floored
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(2), 2); // 2 -> 5 / 2 Floored
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(3), 3);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(4), 3);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(5), 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameNumberTransformerTest_RatesEqual,
	"ProductFilter.CaptureData.FrameNumberTransformer.RatesEqual",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameNumberTransformerTest_RatesEqual::RunTest(const FString& InParameters)
{
	const FFrameRate SourceFrameRate(25'000, 1'000);
	const FFrameRate TargetFrameRate(25'000, 1'000);
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate);

	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(0), 0);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(1), 1);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(2), 2);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(3), 3);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(4), 4);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(5), 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrameNumberTransformerTest_RatesEqualWithOffset,
	"ProductFilter.CaptureData.FrameNumberTransformer.RatesEqualWithOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFrameNumberTransformerTest_RatesEqualWithOffset::RunTest(const FString& InParameters)
{
	constexpr int32 FrameNumberOffset = 3;
	const FFrameRate SourceFrameRate(25'000, 1'000);
	const FFrameRate TargetFrameRate(25'000, 1'000);
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate, FrameNumberOffset);

	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(0), 3);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(1), 4);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(2), 5);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(3), 6);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(4), 7);
	UTEST_EQUAL("Frame Number", FrameNumberTransformer.Transform(5), 8);

	return true;
}

}

#endif // WITH_DEV_AUTOMATION_TESTS