// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "AutoRTFM.h"
#include "Engine/DataTable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoRTFMDataTableTests, "AutoRTFM + UDataTable", EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext)

bool FAutoRTFMDataTableTests::RunTest(const FString & Parameters)
{
	if (!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, TEXT("SKIPPED 'FAutoRTFMDataTableTests' test. AutoRTFM disabled.")));
		return true;
	}

	UDataTable* Object = NewObject<UDataTable>();
	Object->RowStruct = NewObject<UScriptStruct>();

	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		Object->EmptyTable();
		AutoRTFM::AbortTransaction();
	});
	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);

	Result = AutoRTFM::Transact([&]
	{
		Object->EmptyTable();
	});
	TestTrueExpr(AutoRTFM::ETransactionResult::Committed == Result);

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
