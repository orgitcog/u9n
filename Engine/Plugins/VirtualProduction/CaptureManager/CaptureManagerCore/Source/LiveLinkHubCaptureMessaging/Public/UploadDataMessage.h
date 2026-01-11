// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Containers/StaticArray.h"

#include "Internationalization/Text.h"

#include "Templates/ValueOrError.h"

#include "Network/TcpReaderWriter.h"

#include "Misc/Guid.h"

class LIVELINKHUBCAPTUREMESSAGING_API FUploadError
{
public:

	FUploadError(FText InMessage, int32 InCode = 0);

	const FText& GetText() const;
	int32 GetCode() const;

private:

	FText Message;
	int32 Code;
};

template<typename T>
using FUploadResult = TValueOrError<T, FUploadError>;

using FUploadVoidResult = FUploadResult<void>;

struct FUploadDataHeader
{
	static const TArray<uint8> Header;

	FGuid ClientId;
	FGuid CaptureSourceId;
	FGuid TakeUploadId;

	FString CaptureSourceName;
	FString Slate;
	uint32 TakeNumber = 0;
	uint64 TotalLength = 0;
};

struct FUploadFileDataHeader
{
	FString FileName;
	uint64 Length = 0;
};

class LIVELINKHUBCAPTUREMESSAGING_API FUploadDataMessage
{
public:

	static constexpr int32 HashSize = 16;

	static FUploadVoidResult SerializeHeader(FUploadDataHeader InHeader, UE::CaptureManager::ITcpSocketWriter& InWriter);
	static FUploadVoidResult SerializeFileHeader(FUploadFileDataHeader InFileHeader, UE::CaptureManager::ITcpSocketWriter& InWriter);
	static FUploadVoidResult SerializeData(TArray<uint8> InData, UE::CaptureManager::ITcpSocketWriter& InWriter);
	static FUploadVoidResult SerializeHash(TStaticArray<uint8, HashSize> InHash, UE::CaptureManager::ITcpSocketWriter& InWriter);

	static FUploadResult<FUploadDataHeader> DeserializeHeader(UE::CaptureManager::ITcpSocketReader& InReader);
	static FUploadResult<FUploadFileDataHeader> DeserializeFileHeader(UE::CaptureManager::ITcpSocketReader& InReader);
	static FUploadResult<TArray<uint8>> DeserializeData(uint32 InSize, UE::CaptureManager::ITcpSocketReader& InReader);
	static FUploadResult<TStaticArray<uint8, HashSize>> DeserializeHash(UE::CaptureManager::ITcpSocketReader& InReader);

private:

	static FUploadVoidResult DeserializeStartHeader(UE::CaptureManager::ITcpSocketReader& InReader);
	static FUploadVoidResult DeserializeGuid(UE::CaptureManager::ITcpSocketReader& InReader, FGuid& OutFGuid);
	static FUploadVoidResult DeserializeTakeNumber(UE::CaptureManager::ITcpSocketReader& InReader, uint32& OutTakeNumber);
	static FUploadVoidResult DeserializeString(UE::CaptureManager::ITcpSocketReader& InReader, FString& OutTakeName);
	static FUploadVoidResult DeserializeTotalLength(UE::CaptureManager::ITcpSocketReader& InReader, uint64& OutTotalLength);
	static FUploadVoidResult DeserializeFileName(UE::CaptureManager::ITcpSocketReader& InReader, FString& OutFileName);
	static FUploadVoidResult DeserializeLength(UE::CaptureManager::ITcpSocketReader& InReader, uint64& OutLength);

	static constexpr uint32 InactivityTimeout = 15 * 1000; // Miliseconds
};
