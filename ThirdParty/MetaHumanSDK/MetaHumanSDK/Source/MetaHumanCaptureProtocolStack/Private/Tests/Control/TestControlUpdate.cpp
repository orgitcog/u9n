// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Messages/ControlUpdate.h"

#include "Control/Messages/ControlJsonUtilities.h"
#include "Control/Messages/Constants.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanSessionStoppedUpdateTest, "MetaHumanCaptureProtocolStack.Control.ControlUpdate.SessionStopped.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanSessionStoppedUpdateTest::RunTest(const FString& InParameters)
{
    TSharedPtr<FJsonObject> Body;

    FSessionStopped Update;

    TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GSessionStopped);
    TestTrue(TEXT("Parse"), Update.Parse(Body).IsValid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanTakeAddedUpdateTest, "MetaHumanCaptureProtocolStack.Control.ControlUpdate.TakeAdded.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanTakeAddedUpdateTest::RunTest(const FString& InParameters)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(UE::CPS::Properties::GName, TEXT("TakeName"));

	FTakeAddedUpdate Update;

	TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GTakeAdded);

	TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
	TestTrue(TEXT("Parse"), UpdateParseResult.IsValid());
	TestEqual(TEXT("Parse"), Update.GetTakeName(), TEXT("TakeName"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanTakeRemovedUpdateTest, "MetaHumanCaptureProtocolStack.Control.ControlUpdate.TakeRemoved.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanTakeRemovedUpdateTest::RunTest(const FString& InParameters)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(UE::CPS::Properties::GName, TEXT("TakeName"));

	FTakeRemovedUpdate Update;

	TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GTakeRemoved);

	TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
	TestTrue(TEXT("Parse"), UpdateParseResult.IsValid());
	TestEqual(TEXT("Parse"), Update.GetTakeName(), TEXT("TakeName"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanTakeUpdatedUpdateTest, "MetaHumanCaptureProtocolStack.Control.ControlUpdate.TakeUpdated.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanTakeUpdatedUpdateTest::RunTest(const FString& InParameters)
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(UE::CPS::Properties::GName, TEXT("TakeName"));

	FTakeUpdatedUpdate Update;

	TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GTakeUpdated);

	TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
	TestTrue(TEXT("Parse"), UpdateParseResult.IsValid());
	TestEqual(TEXT("Parse"), Update.GetTakeName(), TEXT("TakeName"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanRecordingStatusUpdateTest, "MetaHumanCaptureProtocolStack.Control.ControlUpdate.RecordingStatus.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanRecordingStatusUpdateTest::RunTest(const FString& InParameters)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetBoolField(UE::CPS::Properties::GIsRecording, true);

    FRecordingStatusUpdate Update;

    TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GRecordingStatus);

    TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
    TestTrue(TEXT("Parse"), UpdateParseResult.IsValid());
    TestEqual(TEXT("Parse"), Update.IsRecording(), true);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiskCapacityUpdateTest, "MetaHumanCaptureProtocolStack.Control.ControlUpdate.DiskCapacity.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiskCapacityUpdateTest::RunTest(const FString& InParameters)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetNumberField(UE::CPS::Properties::GTotal, 1024);
    Body->SetNumberField(UE::CPS::Properties::GRemaining, 512);

    FDiskCapacityUpdate Update;

    TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GDiskCapacity);

    TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
    TestTrue(TEXT("Parse"), UpdateParseResult.IsValid());
    TestEqual(TEXT("Parse"), Update.GetTotal(), (uint64)1024);
    TestEqual(TEXT("Parse"), Update.GetRemaining(), (uint64)512);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanBatteryPercentageUpdateTest, "MetaHumanCaptureProtocolStack.Control.ControlUpdate.BatteryPercentage.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanBatteryPercentageUpdateTest::RunTest(const FString& InParameters)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetNumberField(UE::CPS::Properties::GLevel, 100.0);

    FBatteryPercentageUpdate Update;

    TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GBattery);

    TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
    TestTrue(TEXT("Parse"), UpdateParseResult.IsValid());
    TestEqual(TEXT("Parse"), Update.GetLevel(), (float) 100.0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanThermalStateUpdateTest, "MetaHumanCaptureProtocolStack.Control.ControlUpdate.ThermalState.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanThermalStateUpdateTest::RunTest(const FString& InParameters)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();

    Body->SetStringField(UE::CPS::Properties::GState, TEXT("nominal"));

    FThermalStateUpdate Update;

    TestEqual(TEXT("GetAddresPath"), Update.GetAddressPath(), UE::CPS::AddressPaths::GThermalState);

    TProtocolResult<void> UpdateParseResult = Update.Parse(Body);
    TestTrue(TEXT("Parse"), UpdateParseResult.IsValid());
    TestEqual(TEXT("Parse"), Update.GetState(), FThermalStateUpdate::EState::Nominal);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS