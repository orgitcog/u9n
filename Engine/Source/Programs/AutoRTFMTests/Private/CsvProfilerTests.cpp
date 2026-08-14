// Copyright Epic Games, Inc. All Rights Reserved.

#include "API.h"
#include "Async/TaskGraphInterfaces.h"
#include "AutoRTFMTesting.h"
#include "Catch2Includes.h"
#include "CoreTypes.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"

#if !CSV_PROFILER
#error Test relies on CsvProfiler!
#endif

CSV_DEFINE_CATEGORY(MyFalseCategory, false);
CSV_DEFINE_CATEGORY(MyTrueCategory, true);

CSV_DEFINE_CATEGORY(MyIntCategory, true);
CSV_DEFINE_CATEGORY(MyFloatCategory, true);
CSV_DEFINE_CATEGORY(MyDoubleCategory, true);

TEST_CASE("CsvProfiler")
{
	struct RAII final
	{
		RAII()
		{
			FCsvProfiler::Get()->BeginCapture();
			FCsvProfiler::Get()->BeginFrame();
		}

		~RAII()
		{
			FCsvProfiler::Get()->EndFrame();
			FCsvProfiler::Get()->EndCapture();
		}
	} _;

	AutoRTFM::Testing::Abort([&]
	{
		CSV_EVENT(MyFalseCategory, TEXT("HEREWEGO"));
		AutoRTFM::AbortTransaction();
	});

	AutoRTFM::Testing::Commit([&]
	{
		CSV_EVENT(MyFalseCategory, TEXT("HEREWEGO"));
	});

	AutoRTFM::Testing::Abort([&]
	{
		CSV_EVENT(MyTrueCategory, TEXT("HEREWEGO"));
		AutoRTFM::AbortTransaction();
	});

	AutoRTFM::Testing::Commit([&]
	{
		CSV_EVENT(MyTrueCategory, TEXT("HEREWEGO"));
	});

	AutoRTFM::Testing::Abort([&]
	{
		CSV_EVENT(MyFalseCategory, TEXT("HEREWEGO"));
		CSV_SCOPED_TIMING_STAT(MyFalseCategory, Event);
		AutoRTFM::AbortTransaction();
	});

	AutoRTFM::Testing::Commit([&]
	{
		CSV_EVENT(MyFalseCategory, TEXT("HEREWEGO"));
		CSV_SCOPED_TIMING_STAT(MyFalseCategory, Event);
	});

	AutoRTFM::Testing::Abort([&]
	{
		CSV_EVENT(MyTrueCategory, TEXT("HEREWEGO"));
		CSV_SCOPED_TIMING_STAT(MyTrueCategory, Event);
		AutoRTFM::AbortTransaction();
	});

	AutoRTFM::Testing::Commit([&]
	{
		CSV_EVENT(MyTrueCategory, TEXT("HEREWEGO"));
		CSV_SCOPED_TIMING_STAT(MyTrueCategory, Event);
	});

	AutoRTFM::Testing::Commit([&]
	{
		CSV_CUSTOM_STAT(MyFalseCategory, SomeStat, 42, ECsvCustomStatOp::Max);
	});

	AutoRTFM::Testing::Commit([&]
	{
		CSV_CUSTOM_STAT(MyTrueCategory, SomeStat, 42, ECsvCustomStatOp::Max);
	});

	AutoRTFM::Testing::Commit([&]
	{
		CSV_CUSTOM_STAT_GLOBAL(SomeStat, 42, ECsvCustomStatOp::Max);
	});

	AutoRTFM::Testing::Commit([&]
	{
		CSV_CUSTOM_STAT_GLOBAL(SomeStat, 42, ECsvCustomStatOp::Max);
	});

	AutoRTFM::Testing::Commit([&]
	{
		FName Name(TEXT("Wowwee"));
		FCsvProfiler::RecordCustomStat(Name, CSV_CATEGORY_INDEX(MyIntCategory), 42, ECsvCustomStatOp::Accumulate);
	});

	AutoRTFM::Testing::Commit([&]
	{
		FCsvProfiler::RecordCustomStat("Wowwee", CSV_CATEGORY_INDEX(MyIntCategory), 42, ECsvCustomStatOp::Accumulate);
	});

	AutoRTFM::Testing::Commit([&]
	{
		FName Name(TEXT("Wowwee"));
		FCsvProfiler::RecordCustomStat(Name, CSV_CATEGORY_INDEX(MyFloatCategory), static_cast<float>(42), ECsvCustomStatOp::Accumulate);
	});

	AutoRTFM::Testing::Commit([&]
	{
		FCsvProfiler::RecordCustomStat("Wowwee", CSV_CATEGORY_INDEX(MyFloatCategory), static_cast<float>(42), ECsvCustomStatOp::Accumulate);
	});

	AutoRTFM::Testing::Commit([&]
	{
		FName Name(TEXT("Wowwee"));
		FCsvProfiler::RecordCustomStat(Name, CSV_CATEGORY_INDEX(MyDoubleCategory), static_cast<double>(42), ECsvCustomStatOp::Accumulate);
	});

	AutoRTFM::Testing::Commit([&]
	{
		FCsvProfiler::RecordCustomStat("Wowwee", CSV_CATEGORY_INDEX(MyDoubleCategory), static_cast<double>(42), ECsvCustomStatOp::Accumulate);
	});
}
