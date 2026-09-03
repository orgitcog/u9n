// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExportClient/Messages/ExportRequest.h"

#include "Misc/AutomationTest.h"

#include "Tests/Utility.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportRequestDeserializeOneTest, "MetaHumanCaptureProtocolStack.Export.ExportRequest.DeserializeOne.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportRequestDeserializeOneTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const FString TakeName = TEXT("TakeName");
	const FString FileName = TEXT("FileName");
	const uint16 TakeNameLen = TakeName.Len();
	const uint16 FileNameLen = FileName.Len();

	const uint64 Offset = 0;

	TArray<uint8> Packet;
	Packet.Append(reinterpret_cast<const uint8*>(&TakeNameLen), sizeof(TakeNameLen));
	Packet.Append(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*TakeName)), TakeName.Len());
	Packet.Append(reinterpret_cast<const uint8*>(&FileNameLen), sizeof(FileNameLen));
	Packet.Append(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*FileName)), FileName.Len());
	Packet.Append(reinterpret_cast<const uint8*>(&Offset), sizeof(Offset));

	FDataProvider DataProvider(MoveTemp(Packet));

	// Deserialization
	TProtocolResult<FExportRequest> ExportRequestResult = FExportRequest::Deserialize(DataProvider);
	TestTrue(TEXT("Request Deserialize"), ExportRequestResult.IsValid());

	FExportRequest ExportRequest = ExportRequestResult.ClaimResult();

	TestEqual(TEXT("Request Deserialize"), ExportRequest.GetTakeName(), TakeName);
	TestEqual(TEXT("Request Deserialize"), ExportRequest.GetFileName(), FileName);
	TestEqual(TEXT("Request Deserialize"), ExportRequest.GetOffset(), Offset);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportRequestDeserializeOneTestInvalidSize, "MetaHumanCaptureProtocolStack.Export.ExportRequest.DeserializeOne.InvalidSize", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportRequestDeserializeOneTestInvalidSize::RunTest(const FString& InParameters)
{
	// Prepare test
	TArray<uint8> Packet;

	FDataProvider DataProvider(MoveTemp(Packet));

	// Deserialization
	TProtocolResult<FExportRequest> ExportRequestResult = FExportRequest::Deserialize(DataProvider);
	TestTrue(TEXT("Request Deserialize"), ExportRequestResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportRequestSerializeOneTest, "MetaHumanCaptureProtocolStack.Export.ExportRequest.SerializeOne.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportRequestSerializeOneTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const FString TakeName = TEXT("TakeName");
	const FString FileName = TEXT("FileName");
	const uint16 TakeNameLen = TakeName.Len();
	const uint16 FileNameLen = FileName.Len();

	const uint64 Offset = 0;

	TArray<uint8> Packet;
	Packet.Append(reinterpret_cast<const uint8*>(&TakeNameLen), sizeof(TakeNameLen));
	Packet.Append(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*TakeName)), TakeName.Len());
	Packet.Append(reinterpret_cast<const uint8*>(&FileNameLen), sizeof(FileNameLen));
	Packet.Append(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*FileName)), FileName.Len());
	Packet.Append(reinterpret_cast<const uint8*>(&Offset), sizeof(Offset));

	FDataSender DataSender;

	FExportRequest Request(TakeName, FileName, Offset);

	// Serialization
	TProtocolResult<void> ExportRequestResult = FExportRequest::Serialize(Request, DataSender);
	TestTrue(TEXT("Request Serialize"), ExportRequestResult.IsValid());

	TestEqual(TEXT("Request Serialize"), DataSender.GetData(), Packet);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportRequestSerializeOneTestError, "MetaHumanCaptureProtocolStack.Export.ExportRequest.SerializeOne.Error", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportRequestSerializeOneTestError::RunTest(const FString& InParameters)
{
	// Prepare test
	const FString TakeName = TEXT("TakeName");
	const FString FileName = TEXT("FileName");
	const uint16 TakeNameLen = TakeName.Len();
	const uint16 FileNameLen = FileName.Len();

	const uint64 Offset = 0;

	TArray<uint8> Packet;
	Packet.Append(reinterpret_cast<const uint8*>(&TakeNameLen), sizeof(TakeNameLen));
	Packet.Append(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*TakeName)), TakeName.Len());
	Packet.Append(reinterpret_cast<const uint8*>(&FileNameLen), sizeof(FileNameLen));
	Packet.Append(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*FileName)), FileName.Len());
	Packet.Append(reinterpret_cast<const uint8*>(&Offset), sizeof(Offset));

	FFailedDataSender DataSender;

	FExportRequest Request(TakeName, FileName, Offset);

	// Serialization
	TProtocolResult<void> ExportRequestResult = FExportRequest::Serialize(Request, DataSender);
	TestTrue(TEXT("Request Serialize"), ExportRequestResult.IsError());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS