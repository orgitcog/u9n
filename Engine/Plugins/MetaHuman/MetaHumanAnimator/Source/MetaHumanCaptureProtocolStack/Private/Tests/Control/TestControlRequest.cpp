// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Messages/ControlRequest.h"

#include "Control/Messages/ControlJsonUtilities.h"
#include "Control/Messages/Constants.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanGetServerInformationTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.GetServerInformation.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanGetServerInformationTest::RunTest(const FString& InParameters)
{
	FGetServerInformationRequest Request;

	TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GGetServerInformation);
	TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanKeepAliveRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.KeepAlive.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanKeepAliveRequestTest::RunTest(const FString& InParameters)
{        
    FKeepAliveRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GKeepAlive);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanStartSessionRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.StartSession.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanStartSessionRequestTest::RunTest(const FString& InParameters)
{
    FStartSessionRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GStartSession);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanStopSessionRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.StopSession.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanStopSessionRequestTest::RunTest(const FString& InParameters)
{
    FStopSessionRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GStopSession);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanSubscribeRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.Subscribe.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanSubscribeRequestTest::RunTest(const FString& InParameters)
{
    FSubscribeRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GSubscribe);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanUnsubscribeRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.Unsubscribe.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanUnsubscribeRequestTest::RunTest(const FString& InParameters)
{
    FUnsubscribeRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GUnsubscribe);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanGetStateRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.GetState.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanGetStateRequestTest::RunTest(const FString& InParameters)
{
    FGetStateRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GGetState);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanStartRecordingTakeRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.StartRecordingTake.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanStartRecordingTakeRequestTest::RunTest(const FString& InParameters)
{
	FString String = TEXT("{\
        \"slateName\": \"Slate\",\
        \"takeNumber\": 0,\
        \"subject\": \"Subject\",\
        \"scenario\": \"Scenario\",\
        \"tags\": [\"Tag1\", \"Tag2\", \"Tag3\"]\
    }");

	String.ConvertTabsToSpacesInline(2);
	String.RemoveSpacesInline();

	auto ConvertedString = StringCast<UTF8CHAR>(*String);

    FString SlateName = TEXT("Slate");
    FString Subject = TEXT("Subject");
    FString Scenario = TEXT("Scenario");
    TArray<FString> Tags = { TEXT("Tag1"), TEXT("Tag2"), TEXT("Tag3") };
    
    FStartRecordingTakeRequest Request(SlateName, 0, Subject, Scenario, Tags);

    TArray<uint8> Body;
    TestTrue(TEXT("CreateDataFromJson"), FJsonUtility::CreateUTF8DataFromJson(Request.GetBody(), Body));

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GStartRecordingTake);

	TArray<uint8> BodyExpected(reinterpret_cast<const uint8*>(ConvertedString.Get()), ConvertedString.Length());
	TestEqual(TEXT("GetBody"), Body, BodyExpected);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanStopRecordingTakeRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.StopRecordingTake.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanStopRecordingTakeRequestTest::RunTest(const FString& InParameters)
{
	FStopRecordingTakeRequest Request;

	TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GStopRecordingTake);
	TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanAbortRecordingTakeRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.AbortRecordingTake.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanAbortRecordingTakeRequestTest::RunTest(const FString& InParameters)
{
	FAbortRecordingTakeRequest Request;

	TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GAbortRecordingTake);
	TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanGetTakeListRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.GetTakeList.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanGetTakeListRequestTest::RunTest(const FString& InParameters)
{
    FGetTakeListRequest Request;

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GGetTakeList);
    TestFalse(TEXT("GetBody"), Request.GetBody().IsValid()); // Request doesn't have a Body

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanGetTakeMetadataRequestTest, "MetaHumanCaptureProtocolStack.Control.ControlRequest.GetTakeMetadata.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanGetTakeMetadataRequestTest::RunTest(const FString& InParameters)
{
	FString String = TEXT("{\
        \"names\": [\"TakeName1\", \"TakeName2\", \"TakeName3\"]\
    }");

	String.ConvertTabsToSpacesInline(2);
	String.RemoveSpacesInline();

    auto ConvertedString = StringCast<UTF8CHAR>(*String);

    TArray<FString> Takes = { TEXT("TakeName1"), TEXT("TakeName2"), TEXT("TakeName3") };
    FGetTakeMetadataRequest Request(Takes);

    TArray<uint8> Body;
    TestTrue(TEXT("CreateDataFromJson"), FJsonUtility::CreateUTF8DataFromJson(Request.GetBody(), Body));

    TestEqual(TEXT("GetAddresPath"), Request.GetAddressPath(), UE::CPS::AddressPaths::GGetTakeMetadata);

	TArray<uint8> BodyExpected(reinterpret_cast<const uint8*>(ConvertedString.Get()), ConvertedString.Length());
    TestEqual(TEXT("GetBody"), Body, BodyExpected);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS