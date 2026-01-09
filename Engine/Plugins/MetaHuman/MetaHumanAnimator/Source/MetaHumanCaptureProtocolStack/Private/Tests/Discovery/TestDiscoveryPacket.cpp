// Copyright Epic Games, Inc. All Rights Reserved.

#include "Discovery/Communication/DiscoveryPacket.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryPacketDeserializeTest, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryPacket.Deserialize.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryPacketDeserializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;
	const TArray<uint8> Payload = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x00 };

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType) + Payload.Num());
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));
	Packet.Append(Payload);

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.IsValid());

	FDiscoveryPacket DiscoveryPacket = PacketResult.ClaimResult();
	TestEqual(TEXT("Packet Deserialize"), DiscoveryPacket.GetMessageType(), MessageType);
	TestEqual(TEXT("Packet Deserialize"), DiscoveryPacket.GetPayload().Num(), Payload.Num());
	TestEqual(TEXT("Packet Deserialize"), DiscoveryPacket.GetPayload(), Payload);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryPacketDeserializeTestInvalidHeaderSize, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryPacket.Deserialize.InvalidHeaderSize", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryPacketDeserializeTestInvalidHeaderSize::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'A', 'A', 'A', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryPacketDeserializeTestInvalidHeader, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryPacket.Deserialize.InvalidHeader", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryPacketDeserializeTestInvalidHeader::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryPacketDeserializeTestInvalidMessageType, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryPacket.Deserialize.InvalidMessageType", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryPacketDeserializeTestInvalidMessageType::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Invalid;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryPacketSerializeTest, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryPacket.Serialize.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryPacketSerializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	FDiscoveryPacket DiscoveryPacket(MessageType, TArray<uint8>());

	// Deserialization
	TProtocolResult<TArray<uint8>> DataResult = FDiscoveryPacket::Serialize(DiscoveryPacket);
	TestTrue(TEXT("Packet Serialize"), DataResult.IsValid());

	TArray<uint8> Data = DataResult.ClaimResult();
	TestEqual(TEXT("Packet Serialize"), Packet, Data);

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS