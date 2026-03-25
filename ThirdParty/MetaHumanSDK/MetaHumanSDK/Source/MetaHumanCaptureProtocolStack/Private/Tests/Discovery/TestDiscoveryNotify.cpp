// Copyright Epic Games, Inc. All Rights Reserved.

#include "Discovery/Messages/DiscoveryNotify.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryNotifyDeserializeTest, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryNotify.Deserialize.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryNotifyDeserializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Notify;
	const TArray<uint8> ServerId = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f };
	const uint16 ControlPort = 8000;
	const FDiscoveryNotify::EConnectionState ConnectionState = FDiscoveryNotify::EConnectionState::Online;
	const TArray<uint16> SupportedVersions = { 1, 2, 3 };

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType) + ServerId.Num() + sizeof(ControlPort) + SupportedVersions.Num() * sizeof(uint16));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));
	Packet.Append(ServerId);
	Packet.Append((uint8*) &ControlPort, sizeof(ControlPort));
	Packet.Add(static_cast<uint8>(ConnectionState));
	Packet.Append((uint8*)SupportedVersions.GetData(), SupportedVersions.Num() * sizeof(uint16));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.IsValid());

	FDiscoveryPacket DiscoveryPacket = PacketResult.ClaimResult();
	TProtocolResult<FDiscoveryNotify> NotifyResult = FDiscoveryNotify::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Notify Deserialize"), NotifyResult.IsValid());

	FDiscoveryNotify Notify = NotifyResult.ClaimResult();
	int32 ServerIdCompareResult = FMemory::Memcmp(Notify.GetServerId().GetData(), ServerId.GetData(), ServerId.Num());
	TestTrue(TEXT("Notify Deserialize"), (ServerIdCompareResult == 0));
	TestEqual(TEXT("Notify Deserialize"), Notify.GetControlPort(), ControlPort);
	TestEqual(TEXT("Notify Deserialize"), Notify.GetConnectionState(), ConnectionState);
	TestEqual(TEXT("Notify Deserialize"), Notify.GetSupportedVersions(), SupportedVersions);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryNotifyDeserializeTestInvalidMessageType, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryNotify.Deserialize.InvalidMessageType", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryNotifyDeserializeTestInvalidMessageType::RunTest(const FString& InParameters)
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
	TProtocolResult<FDiscoveryNotify> NotifyResult = FDiscoveryNotify::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Notify Deserialize"), NotifyResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryNotifyDeserializeTestInvalidSize, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryNotify.Deserialize.InvalidSize", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryNotifyDeserializeTestInvalidSize::RunTest(const FString& InParameters)
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
	TProtocolResult<FDiscoveryNotify> NotifyResult = FDiscoveryNotify::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Notify Deserialize"), NotifyResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryNotifyDeserializeTestInvalidConnectionState, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryNotify.Deserialize.InvalidConnectionState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryNotifyDeserializeTestInvalidConnectionState::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Notify;
	const TArray<uint8> ServerId = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f };
	const uint16 ControlPort = 8000;
	const FDiscoveryNotify::EConnectionState ConnectionState = FDiscoveryNotify::EConnectionState::Invalid;
	const TArray<uint16> SupportedVersions = { 1, 2, 3 };

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType) + ServerId.Num() + sizeof(ControlPort) + SupportedVersions.Num() * sizeof(uint16));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));
	Packet.Append(ServerId);
	Packet.Append((uint8*) &ControlPort, sizeof(ControlPort));
	Packet.Add(static_cast<uint8>(ConnectionState));
	Packet.Append((uint8*)SupportedVersions.GetData(), SupportedVersions.Num() * sizeof(uint16));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.IsValid());

	FDiscoveryPacket DiscoveryPacket = PacketResult.ClaimResult();
	TProtocolResult<FDiscoveryNotify> NotifyResult = FDiscoveryNotify::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Notify Deserialize"), NotifyResult.IsError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TMetaHumanDiscoveryNotifySerializeTest, "MetaHumanCaptureProtocolStack.Discovery.DiscoveryNotify.Serialize.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TMetaHumanDiscoveryNotifySerializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Notify;

	const TArray<uint8> ServerId = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f };
	const uint16 ControlPort = 8000;
	const FDiscoveryNotify::EConnectionState ConnectionState = FDiscoveryNotify::EConnectionState::Online;
	const TArray<uint16> SupportedVersions = { 1 };

	TArray<uint8> Payload;
	Payload.Reserve(ServerId.Num() + sizeof(ControlPort) + sizeof(ConnectionState) + SupportedVersions.Num() * sizeof(uint16));
	Payload.Append(ServerId);
	Payload.Append((uint8*) &ControlPort, sizeof(ControlPort));
	Payload.Add(static_cast<uint8>(ConnectionState));
	Payload.Append((uint8*) SupportedVersions.GetData(), SupportedVersions.Num() * sizeof(uint16));

	FDiscoveryNotify::FServerId ServerIdStatic;
	FMemory::Memcpy(ServerIdStatic.GetData(), ServerId.GetData(), ServerId.Num());

	FDiscoveryNotify Notify(MoveTemp(ServerIdStatic), ControlPort, ConnectionState, SupportedVersions);

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryNotify::Serialize(Notify);
	TestTrue(TEXT("Packet Serialize"), PacketResult.IsValid());

	FDiscoveryPacket DiscoveryPacket = PacketResult.ClaimResult();
	TestEqual(TEXT("Notify Serialize"), DiscoveryPacket.GetMessageType(), MessageType);
	TestEqual(TEXT("Notify Serialize"), DiscoveryPacket.GetPayload(), Payload);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS