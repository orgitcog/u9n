// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExportClient/Messages/ExportResponse.h"

#include "Misc/AutomationTest.h"

#include "Tests/Utility.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportResponseDeserializeOneTest, "MetaHumanCaptureProtocolStack.Export.ExportResponse.DeserializeOne.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportResponseDeserializeOneTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const FExportResponse::EStatus Status = FExportResponse::EStatus::Success;
	const uint64 Length = 50;

	TArray<uint8> Packet;
	Packet.Append(reinterpret_cast<const uint8*>(&Status), sizeof(Status));
	Packet.Append(reinterpret_cast<const uint8*>(&Length), sizeof(Length));

	FDataProvider DataProvider(MoveTemp(Packet));

	// Deserialization
	TProtocolResult<FExportResponse> ExportResponseResult = FExportResponse::Deserialize(DataProvider);
	TestTrue(TEXT("Response Deserialize"), ExportResponseResult.IsValid());

	FExportResponse ExportResponse = ExportResponseResult.ClaimResult();

	TestEqual(TEXT("Response Deserialize"), ExportResponse.GetStatus(), Status);
	TestEqual(TEXT("Response Deserialize"), ExportResponse.GetLength(), Length);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportResponseDeserializeOneTestInvalidSize, "MetaHumanCaptureProtocolStack.Export.ExportResponse.DeserializeOne.InvalidSize", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportResponseDeserializeOneTestInvalidSize::RunTest(const FString& InParameters)
{
	// Prepare test
	TArray<uint8> Packet;

	FDataProvider DataProvider(MoveTemp(Packet));

	// Deserialization
	TProtocolResult<FExportResponse> ExportResponseResult = FExportResponse::Deserialize(DataProvider);
	TestTrue(TEXT("Response Deserialize"), ExportResponseResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportResponseSerializeOneTest, "MetaHumanCaptureProtocolStack.Export.ExportResponse.SerializeOne.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportResponseSerializeOneTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const FExportResponse::EStatus Status = FExportResponse::EStatus::Success;
	const uint64 Length = 50;

	TArray<uint8> Packet;
	Packet.Append(reinterpret_cast<const uint8*>(&Status), sizeof(Status));
	Packet.Append(reinterpret_cast<const uint8*>(&Length), sizeof(Length));

	FDataSender DataSender;

	FExportResponse Response(Status, Length);

	// Serialization
	TProtocolResult<void> ExportResponseResult = FExportResponse::Serialize(Response, DataSender);
	TestTrue(TEXT("Response Serialize"), ExportResponseResult.IsValid());

	TestEqual(TEXT("Response Serialize"), DataSender.GetData(), Packet);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanExportResponseSerializeOneTestError, "MetaHumanCaptureProtocolStack.Export.ExportResponse.SerializeOne.Error", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanExportResponseSerializeOneTestError::RunTest(const FString& InParameters)
{
	// Prepare test
	const FExportResponse::EStatus Status = FExportResponse::EStatus::Success;
	const uint64 Length = 50;

	TArray<uint8> Packet;
	Packet.Append(reinterpret_cast<const uint8*>(&Status), sizeof(Status));
	Packet.Append(reinterpret_cast<const uint8*>(&Length), sizeof(Length));

	FFailedDataSender DataSender;

	FExportResponse Response(Status, Length);

	// Serialization
	TProtocolResult<void> ExportResponseResult = FExportResponse::Serialize(Response, DataSender);
	TestTrue(TEXT("Response Serialize"), ExportResponseResult.IsError());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS