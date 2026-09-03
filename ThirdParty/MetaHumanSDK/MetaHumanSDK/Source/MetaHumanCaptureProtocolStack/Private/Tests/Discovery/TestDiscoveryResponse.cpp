// Copyright Epic Games, Inc. All Rights Reserved.

#include "Discovery/Messages/DiscoveryResponse.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryResponseDeserializeTest, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryResponse.Deserialize.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryResponseDeserializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Response;
	const TArray<uint8> ServerId = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f };
	const uint16 ControlPort = 8000;
	const TArray<uint16> SupportedVersions = { 1, 2, 3 };

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType) + ServerId.Num() + sizeof(ControlPort) + SupportedVersions.Num() * sizeof(uint16));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));
	Packet.Append(ServerId);
	Packet.Append((uint8*) &ControlPort, sizeof(ControlPort));
	Packet.Append((uint8*)SupportedVersions.GetData(), SupportedVersions.Num() * sizeof(uint16));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.IsValid());

	FDiscoveryPacket DiscoveryPacket = PacketResult.ClaimResult();
	TProtocolResult<FDiscoveryResponse> ResponseResult = FDiscoveryResponse::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Response Deserialize"), ResponseResult.IsValid());

	FDiscoveryResponse Response = ResponseResult.ClaimResult();
	int32 ServerIdCompareResult = FMemory::Memcmp(Response.GetServerId().GetData(), ServerId.GetData(), ServerId.Num());
	TestTrue(TEXT("Response Deserialize"), (ServerIdCompareResult == 0));
	TestEqual(TEXT("Response Deserialize"), Response.GetControlPort(), ControlPort);
	TestEqual(TEXT("Response Deserialize"), Response.GetSupportedVersions(), SupportedVersions);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryResponseDeserializeTestInvalidMessageType, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryResponse.Deserialize.InvalidMessageType", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryResponseDeserializeTestInvalidMessageType::RunTest(const FString& InParameters)
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
	TProtocolResult<FDiscoveryResponse> ResponseResult = FDiscoveryResponse::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Response Deserialize"), ResponseResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryResponseDeserializeTestInvalidSize, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryResponse.Deserialize.InvalidSize", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryResponseDeserializeTestInvalidSize::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Notify;
	const TArray<uint8> ServerId = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f };

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));
	Packet.Append(ServerId);

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.IsValid());

	FDiscoveryPacket DiscoveryPacket = PacketResult.ClaimResult();
	TProtocolResult<FDiscoveryResponse> ResponseResult = FDiscoveryResponse::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Response Deserialize"), ResponseResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryResponseSerializeTest, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryResponse.Serialize.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryResponseSerializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Response;

	const TArray<uint8> ServerId = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f };
	const uint16 ControlPort = 8000;
	const TArray<uint16> SupportedVersions = { 1 };

	TArray<uint8> Payload;
	Payload.Reserve(ServerId.Num() + sizeof(ControlPort) + SupportedVersions.Num() * sizeof(uint16));
	Payload.Append(ServerId);
	Payload.Append((uint8*) &ControlPort, sizeof(ControlPort));
	Payload.Append((uint8*) SupportedVersions.GetData(), SupportedVersions.Num() * sizeof(uint16));

	FDiscoveryResponse::FServerId ServerIdStatic;
	FMemory::Memcpy(ServerIdStatic.GetData(), ServerId.GetData(), ServerId.Num());

	FDiscoveryResponse Response(MoveTemp(ServerIdStatic), ControlPort, SupportedVersions);

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryResponse::Serialize(Response);
	TestTrue(TEXT("Packet Serialize"), PacketResult.IsValid());

	FDiscoveryPacket DiscoveryPacket = PacketResult.ClaimResult();
	TestEqual(TEXT("Response Serialize"), DiscoveryPacket.GetMessageType(), MessageType);
	TestEqual(TEXT("Response Serialize"), DiscoveryPacket.GetPayload(), Payload);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS