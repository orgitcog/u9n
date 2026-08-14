// Copyright Epic Games, Inc. All Rights Reserved.

#include "Discovery/Messages/DiscoveryRequest.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryRequestDeserializeTest, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryRequest.Deserialize.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryRequestDeserializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.IsValid());

	FDiscoveryPacket DiscoveryPacket = PacketResult.ClaimResult();
	TProtocolResult<FDiscoveryRequest> RequestResult = FDiscoveryRequest::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Request Deserialize"), RequestResult.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryRequestDeserializeTestInvalidMessageType, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryRequest.Deserialize.InvalidMessageType", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryRequestDeserializeTestInvalidMessageType::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Response;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.IsValid());

	FDiscoveryPacket DiscoveryPacket = PacketResult.ClaimResult();

	TProtocolResult<FDiscoveryRequest> RequestResult = FDiscoveryRequest::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Request Deserialize"), RequestResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryRequestSerializeTest, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryRequest.Serialize.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryRequestSerializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;

	FDiscoveryRequest Request;

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryRequest::Serialize(Request);
	TestTrue(TEXT("Request Serialize"), PacketResult.IsValid());

	FDiscoveryPacket DiscoveryPacket = PacketResult.ClaimResult();
	TestEqual(TEXT("Request Serialize"), DiscoveryPacket.GetMessageType(), MessageType);
	TestTrue(TEXT("Request Serialize"), DiscoveryPacket.GetPayload().IsEmpty());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS