// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Control/Communication/ControlPacket.h"

namespace UE::CaptureManager
{

class CAPTUREPROTOCOLSTACK_API FControlMessage final
{
public:

	enum class EType
	{
		Request,
		Response,
		Update,

		Invalid
	};

	FControlMessage(FString InAddressPath, EType InType, TSharedPtr<FJsonObject> InBody);

	FControlMessage(const FControlMessage& InOther) = default;
	FControlMessage(FControlMessage&& InOther) = default;

	FControlMessage& operator=(const FControlMessage& InOther) = default;
	FControlMessage& operator=(FControlMessage&& InOther) = default;

	static TProtocolResult<FControlMessage> Deserialize(const FControlPacket& InPacket);
	static TProtocolResult<FControlPacket> Serialize(const FControlMessage& InMessage);

	void SetSessionId(FString InSessionId);
	void SetTransactionId(uint32 InTrasactionId);
	void SetTimestamp(uint64 InTimestamp);

	const FString& GetSessionId() const;
	const FString& GetAddressPath() const;
	uint32 GetTransactionId() const;
	uint64 GetTimestamp() const;
	EType GetType() const;
	const TSharedPtr<FJsonObject>& GetBody() const;
	TSharedPtr<FJsonObject>& GetBody();

	const FString& GetErrorName() const;
	const FString& GetErrorDescription() const;

private:

	struct FErrorResponse
	{
		FString Name = TEXT("");
		FString Description = TEXT("");
	};

	FControlMessage(FString InSessionId,
					FString InAddressPath,
					uint32 InTransactionId,
					uint64 InTimestamp,
					EType InMessageType,
					TSharedPtr<FJsonObject> InBody,
					FErrorResponse InError);

	static EType DeserializeType(FString InMessageTypeStr);
	static FString SerializeType(EType InMessageType);

	FString SessionId;
	FString AddressPath;
	uint32 TransactionId = 0;
	uint64 Timestamp = 0;
	EType MessageType;
	TSharedPtr<FJsonObject> Body;
	FErrorResponse Error;
};

}