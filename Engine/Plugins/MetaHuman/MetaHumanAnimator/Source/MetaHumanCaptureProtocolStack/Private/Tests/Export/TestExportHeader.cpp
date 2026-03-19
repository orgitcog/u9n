// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExportClient/Communication/ExportHeader.h"

#include "Misc/AutomationTest.h"

#include "Tests/Utility.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportHeaderDeserializeOneTest, "MetaHumanCaptureProtocolStack.Export.ExportHeader.DeserializeOne.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportHeaderDeserializeOneTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'E', 'X', 'P', 'O', 'R', 'T', '\0' };
	const uint16 Version = 1;
	const uint32 TransactionId = 11223344;

	TArray<uint8> Data;
	Data.Append(Header);
	Data.Append(reinterpret_cast<const uint8*>(&Version), sizeof(Version));
	Data.Append(reinterpret_cast<const uint8*>(&TransactionId), sizeof(TransactionId));

	FDataProvider DataProvider(MoveTemp(Data));

	// Deserialization
	TProtocolResult<FExportHeader> ExportHeaderResult = FExportHeader::Deserialize(DataProvider);
	TestTrue(TEXT("Header Deserialize"), ExportHeaderResult.IsValid());

	FExportHeader ExportHeader = ExportHeaderResult.ClaimResult();

	TestEqual(TEXT("Header Deserialize"), ExportHeader.GetVersion(), Version);
	TestEqual(TEXT("Header Deserialize"), ExportHeader.GetTransactionId(), TransactionId);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportHeaderDeserializeOneTestInvalidHeader, "MetaHumanCaptureProtocolStack.Export.ExportHeader.DeserializeOne.InvalidHeader", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportHeaderDeserializeOneTestInvalidHeader::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', '\0' };
	const uint16 Version = 1;
	const uint32 TransactionId = 11223344;

	TArray<uint8> Data;
	Data.Append(Header);
	Data.Append(reinterpret_cast<const uint8*>(&Version), sizeof(Version));
	Data.Append(reinterpret_cast<const uint8*>(&TransactionId), sizeof(TransactionId));

	FDataProvider DataProvider(MoveTemp(Data));

	// Deserialization
	TProtocolResult<FExportHeader> ExportHeaderResult = FExportHeader::Deserialize(DataProvider);
	TestTrue(TEXT("Header Deserialize"), ExportHeaderResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportHeaderDeserializeOneTestInvalidSize, "MetaHumanCaptureProtocolStack.Export.ExportHeader.DeserializeOne.InvalidSize", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportHeaderDeserializeOneTestInvalidSize::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'E', 'X', 'P', 'O', 'R', 'T', '\0' };

	TArray<uint8> Data;
	Data.Reserve(Header.Num());
	Data.Append(Header);

	FDataProvider DataProvider(MoveTemp(Data));

	// Deserialization
	TProtocolResult<FExportHeader> ExportHeaderResult = FExportHeader::Deserialize(DataProvider);
	TestTrue(TEXT("Header Deserialize"), ExportHeaderResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportHeaderSerializeOneTest, "MetaHumanCaptureProtocolStack.Export.ExportHeader.SerializeOne.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportHeaderSerializeOneTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'E', 'X', 'P', 'O', 'R', 'T', '\0' };
	const uint16 Version = 1;
	const uint32 TransactionId = 11223344;

	TArray<uint8> Data;
	Data.Append(Header);
	Data.Append(reinterpret_cast<const uint8*>(&Version), sizeof(Version));
	Data.Append(reinterpret_cast<const uint8*>(&TransactionId), sizeof(TransactionId));

	FDataSender DataSender;

	// Serialization
	FExportHeader ExportHeader(Version, TransactionId);
	TProtocolResult<void> ExportHeaderResult = FExportHeader::Serialize(ExportHeader, DataSender);
	TestTrue(TEXT("Header Serialize"), ExportHeaderResult.IsValid());

	TestEqual(TEXT("Header Serialize"), DataSender.GetData(), Data);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportHeaderSerializeOneTestError, "MetaHumanCaptureProtocolStack.Export.ExportHeader.SerializeOne.Error", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportHeaderSerializeOneTestError::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'E', 'X', 'P', 'O', 'R', 'T', '\0' };
	const uint16 Version = 1;
	const uint32 TransactionId = 11223344;

	TArray<uint8> Data;
	Data.Append(Header);
	Data.Append(reinterpret_cast<const uint8*>(&Version), sizeof(Version));
	Data.Append(reinterpret_cast<const uint8*>(&TransactionId), sizeof(TransactionId));

	FFailedDataSender DataSender;

	// Serialization
	FExportHeader ExportHeader(Version, TransactionId);
	TProtocolResult<void> ExportHeaderResult = FExportHeader::Serialize(ExportHeader, DataSender);
	TestTrue(TEXT("Header Serialize"), ExportHeaderResult.IsError());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS