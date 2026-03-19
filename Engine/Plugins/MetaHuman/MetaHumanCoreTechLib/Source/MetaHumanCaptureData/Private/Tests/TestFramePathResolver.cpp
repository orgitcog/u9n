// Copyright Epic Games, Inc. All Rights Reserved.

#include "FramePathResolver.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::MetaHuman::Private
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFramePathResolverTest_NoTransform,
	"MetaHuman.Capture.CaptureData.FramePathResolver.NoTransform",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFramePathResolverTest_NoTransform::RunTest(const FString& InParameters)
{
	const FFramePathResolver FramePathResolver(TEXT("/Some/Path/Frame_%05d.png"));

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(0);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00000.png");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(20);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00020.png");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFramePathResolverTest_SimpleOffsetTransform,
	"MetaHuman.Capture.CaptureData.FramePathResolver.SimpleOffsetTransform",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFramePathResolverTest_SimpleOffsetTransform::RunTest(const FString& InParameters)
{
	constexpr int32 FrameNumberOffset = -6;
	FFrameNumberTransformer FrameNumberTransformer(FrameNumberOffset);
	const FFramePathResolver FramePathResolver(TEXT("/Some/Path/Frame_%05d.gif"), MoveTemp(FrameNumberTransformer));

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(6);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00000.gif");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(20);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00014.gif");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFramePathResolverTest_TransformWithHigherTargetRate,
	"MetaHuman.Capture.CaptureData.FramePathResolver.TransformWithHigherTargetRate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFramePathResolverTest_TransformWithHigherTargetRate::RunTest(const FString& InParameters)
{
	const FFrameRate SourceFrameRate(30'000, 1'000);
	const FFrameRate TargetFrameRate(SourceFrameRate.Numerator * 2, SourceFrameRate.Denominator);
	constexpr int32 FrameNumberOffset = 0;
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate, FrameNumberOffset);

	const FFramePathResolver FramePathResolver(TEXT("/Some/Path/Frame_%05d.jpg"), MoveTemp(FrameNumberTransformer));

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(0);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00000.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(22);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00011.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(23);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00011.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(24);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00012.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFramePathResolverTest_TransformWithHigherTargetRateAndOffset,
	"MetaHuman.Capture.CaptureData.FramePathResolver.TransformWithHigherTargetRateAndOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFramePathResolverTest_TransformWithHigherTargetRateAndOffset::RunTest(const FString& InParameters)
{
	const FFrameRate SourceFrameRate(20'000, 1'000);
	const FFrameRate TargetFrameRate(SourceFrameRate.Numerator * 3, SourceFrameRate.Denominator);

	constexpr int32 FrameNumberOffset = 12;
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate, FrameNumberOffset);
	const FFramePathResolver FramePathResolver(TEXT("/Some/Path/Frame_%05d.exr"), MoveTemp(FrameNumberTransformer));

	{
		// We ask for frame 0, which gets mapped to frame 12 in the target rate, which gets reduced to 4 in the source rate
		const FString ResolvedPath = FramePathResolver.ResolvePath(0);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00004.exr");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(1);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00004.exr");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(2);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00004.exr");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(3);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00005.exr");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFramePathResolverTest_TransformWithHigherTargetRateAndNegativeOffset,
	"MetaHuman.Capture.CaptureData.FramePathResolver.TransformWithHigherTargetRateAndNegativeOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFramePathResolverTest_TransformWithHigherTargetRateAndNegativeOffset::RunTest(const FString& InParameters)
{
	const FFrameRate SourceFrameRate(30'000, 1'000);
	const FFrameRate TargetFrameRate(SourceFrameRate.Numerator * 2, SourceFrameRate.Denominator);

	constexpr int32 FrameNumberOffset = -500;
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate, FrameNumberOffset);
	const FFramePathResolver FramePathResolver(TEXT("/Some/Path/Frame_%05d.jpg"), MoveTemp(FrameNumberTransformer));

	{
		// We ask for frame 500, which gets mapped to frame 0 in the target rate, which then gets halved to 0 in the source rate.
		const FString ResolvedPath = FramePathResolver.ResolvePath(500);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00000.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		// We ask for frame 501, which gets mapped to frame 1 in the target rate, which then gets halved 0 in the source rate.
		const FString ResolvedPath = FramePathResolver.ResolvePath(501);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00000.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		// We ask for frame 502, which gets mapped to frame 2 in the target rate, which then gets halved 1 in the source rate.
		const FString ResolvedPath = FramePathResolver.ResolvePath(502);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00001.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		// We ask for frame 503, which gets mapped to frame 3 in the target rate, which then gets halved 1 in the source rate.
		const FString ResolvedPath = FramePathResolver.ResolvePath(503);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00001.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFramePathResolverTest_TransformWithLowerTargetRate,
	"MetaHuman.Capture.CaptureData.FramePathResolver.TransformWithLowerTargetRate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFramePathResolverTest_TransformWithLowerTargetRate::RunTest(const FString& InParameters)
{
	const FFrameRate SourceFrameRate(60'000, 1'000);
	const FFrameRate TargetFrameRate(SourceFrameRate.Numerator / 2, SourceFrameRate.Denominator);

	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate);
	const FFramePathResolver FramePathResolver(TEXT("/Some/Path/Frame_%05d.jpg"), MoveTemp(FrameNumberTransformer));

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(0);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00000.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(1);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00002.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(3);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00006.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(4);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00008.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFramePathResolverTest_TransformWithLowerTargetRateAndOffset,
	"MetaHuman.Capture.CaptureData.FramePathResolver.TransformWithLowerTargetRateAndOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFramePathResolverTest_TransformWithLowerTargetRateAndOffset::RunTest(const FString& InParameters)
{
	const FFrameRate SourceFrameRate(60'000, 1'000);
	const FFrameRate TargetFrameRate(SourceFrameRate.Numerator / 3, SourceFrameRate.Denominator);

	constexpr int32 FrameNumberOffset = 12;
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate, FrameNumberOffset);
	const FFramePathResolver FramePathResolver(TEXT("/Some/Path/Frame_%05d.exr"), MoveTemp(FrameNumberTransformer));

	{
		// We ask for frame 0, which gets mapped to frame 12 in the target rate, which then gets doubled up to 36 in the source rate.
		const FString ResolvedPath = FramePathResolver.ResolvePath(0);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00036.exr");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(1);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00039.exr");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(2);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00042.exr");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(3);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00045.exr");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFramePathResolverTest_TransformWithLowerTargetRateAndNegativeOffset,
	"MetaHuman.Capture.CaptureData.FramePathResolver.TransformWithLowerTargetRateAndNegativeOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFramePathResolverTest_TransformWithLowerTargetRateAndNegativeOffset::RunTest(const FString& InParameters)
{
	const FFrameRate SourceFrameRate(60'000, 1'000);
	const FFrameRate TargetFrameRate(SourceFrameRate.Numerator / 2, SourceFrameRate.Denominator);

	constexpr int32 FrameNumberOffset = -500;
	FFrameNumberTransformer FrameNumberTransformer(SourceFrameRate, TargetFrameRate, FrameNumberOffset);
	const FFramePathResolver FramePathResolver(TEXT("/Some/Path/Frame_%05d.jpg"), MoveTemp(FrameNumberTransformer));

	{
		// We ask for frame 500, which gets mapped to frame 0 in the target rate, which then gets doubled to 0 in the source rate.
		const FString ResolvedPath = FramePathResolver.ResolvePath(500);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00000.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		// We ask for frame 501, which gets mapped to frame 1 in the target rate, which then gets doubled to 2 in the source rate.
		const FString ResolvedPath = FramePathResolver.ResolvePath(501);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00002.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		// We ask for frame 502, which gets mapped to frame 2 in the target rate, which then gets doubled to 4 in the source rate.
		const FString ResolvedPath = FramePathResolver.ResolvePath(502);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00004.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	{
		// We ask for frame 503, which gets mapped to frame 3 in the target rate, which then gets doubled to 6 in the source rate.
		const FString ResolvedPath = FramePathResolver.ResolvePath(503);
		const FString ExpectedPath = TEXT("/Some/Path/Frame_00006.jpg");
		UTEST_EQUAL("Resolved path", *ResolvedPath, *ExpectedPath);
	}

	return true;
}



}

#endif // WITH_DEV_AUTOMATION_TESTS