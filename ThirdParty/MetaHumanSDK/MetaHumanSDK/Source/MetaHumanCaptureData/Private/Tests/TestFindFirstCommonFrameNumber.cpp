// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencedImageTrackInfo.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::MetaHuman::Private
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_Empty,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.Empty",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_Empty::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, -1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_SameValues,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.SameValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_SameValues::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(1'000, 1'000),
			TRange<FFrameNumber>(4, 18)
		},
		{
			FFrameRate(1'000, 1'000),
			TRange<FFrameNumber>(4, 20)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_DifferentValues,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.DifferentValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_DifferentValues::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(1'000, 1'000),
			TRange<FFrameNumber>(9, 18)
		},
		{
			FFrameRate(1'000, 1'000),
			TRange<FFrameNumber>(4, 16)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 9);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_NoOverlap,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.NoOverlap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_NoOverlap::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(1'000, 1'000),
			TRange<FFrameNumber>(9, 18)
		},
		{
			FFrameRate(1'000, 1'000),
			TRange<FFrameNumber>(24, 36)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, -1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_LowerFrameRateStartsFirst,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.LowerFrameRateStartsFirst",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_LowerFrameRateStartsFirst::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(1, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(0, 1430)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_LowerFrameRateStartsSecond,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.LowerFrameRateStartsSecond",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_LowerFrameRateStartsSecond::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(0, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(1, 1430)
		},
	};
	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	// V: 0, 1, 2, 3
	// D: E, 1, X, 3
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_LowerFrameRateStartsFirstWithOffsetEqualToRatio,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.LowerFrameRateStartsFirstWithOffsetEqualToRatio",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_LowerFrameRateStartsFirstWithOffsetEqualToRatio::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(2, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(0, 1430)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_LowerFrameRateStartsFirstWithOffsetGreaterThanRatio,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.LowerFrameRateStartsFirstWithOffsetGreaterThanRatio",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_LowerFrameRateStartsFirstWithOffsetGreaterThanRatio::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(3, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(0, 1430)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_LowerFrameRateStartsSecondWithOffsetGreaterThanRatio,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.LowerFrameRateStartsSecondWithOffsetGreaterThanRatio",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_LowerFrameRateStartsSecondWithOffsetGreaterThanRatio::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(3, 1431)
		},
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(0, 1430)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_LowerFrameRateStartsSecondWithOffsetEqualToRatio,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.LowerFrameRateStartsSecondWithOffsetEqualToRatio",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_LowerFrameRateStartsSecondWithOffsetEqualToRatio::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(2, 1431)
		},
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(0, 1430)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_NonZeroLowerFrameRateStartsFirstWithOffsetGreaterThanRatio,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.NonZeroLowerFrameRateStartsFirstWithOffsetGreaterThanRatio",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_NonZeroLowerFrameRateStartsFirstWithOffsetGreaterThanRatio::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(6, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(3, 1430)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 7);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_NonZeroLowerFrameRateStartsSecondWithOffsetGreaterThanRatio,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.NonZeroLowerFrameRateStartsSecondWithOffsetGreaterThanRatio",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_NonZeroLowerFrameRateStartsSecondWithOffsetGreaterThanRatio::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(3, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(6, 1430)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_NonZeroLowerFrameRateStartsSecondWithOffsetEqualToRatio,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.NonZeroLowerFrameRateStartsSecondWithOffsetEqualToRatio",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_NonZeroLowerFrameRateStartsSecondWithOffsetEqualToRatio::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(2, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(4, 1430)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_NonZeroLowerFrameRateStartsFirstWithOffsetEqualToRatio,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.NonZeroLowerFrameRateStartsFirstWithOffsetEqualToRatio",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_NonZeroLowerFrameRateStartsFirstWithOffsetEqualToRatio::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(60'000, 1'000),
			TRange<FFrameNumber>(4, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(2, 1430)
		},
	};

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_FrameRatio4x,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.FrameRatio4x",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_FrameRatio4x::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(120'000, 1'000),
			TRange<FFrameNumber>(1, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(1, 1430)
		},
	};
	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_FrameRatio4xLowerFrameRateStartsFirst,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.FrameRatio4xLowerFrameRateStartsFirst",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_FrameRatio4xLowerFrameRateStartsFirst::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(120'000, 1'000),
			TRange<FFrameNumber>(1, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(0, 1430)
		},
	};
	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_FrameRatio4xLowerFrameRateStartsFirstWithOffset,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.FrameRatio4xLowerFrameRateStartsFirstWithOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_FrameRatio4xLowerFrameRateStartsFirstWithOffset::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(120'000, 1'000),
			TRange<FFrameNumber>(2, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(0, 1430)
		},
	};
	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_FrameRatio4xLowerFrameRateStartsFirstWithOffsetGreaterThanRatio,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.FrameRatio4xLowerFrameRateStartsFirstWithOffsetGreaterThanRatio",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_FrameRatio4xLowerFrameRateStartsFirstWithOffsetGreaterThanRatio::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(120'000, 1'000),
			TRange<FFrameNumber>(5, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(0, 1430)
		},
	};
	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_FrameRatio4xLowerFrameRateStartsSecond,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.FrameRatio4xLowerFrameRateStartsSecond",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_FrameRatio4xLowerFrameRateStartsSecond::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(120'000, 1'000),
			TRange<FFrameNumber>(0, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(1, 1430)
		},
	};
	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFindFirstCommonFrameNumberTest_FrameRatio4xLowerFrameRateStartsSecondWithOffset,
	"MetaHuman.Capture.CaptureData.FindFirstCommonFrameNumber.FrameRatio4xLowerFrameRateStartsSecondWithOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFindFirstCommonFrameNumberTest_FrameRatio4xLowerFrameRateStartsSecondWithOffset::RunTest(const FString& InParameters)
{
	const TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos
	{
		{
			FFrameRate(120'000, 1'000),
			TRange<FFrameNumber>(0, 1431)
		},
		{
			FFrameRate(30'000, 1'000),
			TRange<FFrameNumber>(3, 1430)
		},
	};
	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(SequencedImageTrackInfos);
	UTEST_EQUAL("First frame in all tracks", FirstCommonFrameNumber, 3);
	return true;
}

}

#endif // WITH_DEV_AUTOMATION_TESTS
