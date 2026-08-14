// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFMTesting.h"
#include "Misc/AutomationTest.h"
#include "AutoRTFM.h"

#include "Framework/Text/CharRangeList.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoRTFMSlateTests, "AutoRTFM + Slate", EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext)

bool FAutoRTFMSlateTests::RunTest(const FString & Parameters)
{
	if (!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, TEXT("SKIPPED 'FAutoRTFMSlateTests' test. AutoRTFM disabled.")));
		return true;
	}

	// Test for SOL-7842
	AutoRTFM::Testing::Commit([&]
	{
		FCharRangeList CharRangeList;
		TestTrueExpr(CharRangeList.IsEmpty());
		CharRangeList.InitializeFromString(TEXT("a-zA-Z0-9._"));
		TestFalseExpr(CharRangeList.IsEmpty());
	});
	
	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
