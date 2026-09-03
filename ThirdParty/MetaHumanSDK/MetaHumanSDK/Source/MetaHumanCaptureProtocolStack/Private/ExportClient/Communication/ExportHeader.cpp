// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExportClient/Communication/ExportHeader.h"
#include "ExportClient/Definitions.h"

const TArray<uint8> FExportHeader::Header = { 'C', 'P', 'S', 'E', 'X', 'P', 'O', 'R', 'T', '\0' };

#define CPS_CHECK_RESULT_RET_ERROR(Result) if (Result.IsError()) { return Result.ClaimError(); }

FExportHeader::FExportHeader(uint16 InVersion, uint32 InTransactionId)
	: Version(InVersion)
	, TransactionId(InTransactionId)
{
}

TProtocolResult<FExportHeader> FExportHeader::Deserialize(ITcpSocketReader& InReader)
{
	FExportHeader ExportHeader;

	TProtocolResult<TArray<uint8>> HeaderResult = InReader.ReceiveMessage(Header.Num(), InactivityTimeoutMs);
	CPS_CHECK_RESULT_RET_ERROR(HeaderResult);

	TArray<uint8> HeaderData = HeaderResult.ClaimResult();

	if (FMemory::Memcmp(HeaderData.GetData(), Header.GetData(), Header.Num()) != 0)
	{
		return FCaptureProtocolError(TEXT("Header doesn't match"));
	}

	TProtocolResult<TArray<uint8>> VersionResult = InReader.ReceiveMessage(sizeof(ExportHeader.Version), InactivityTimeoutMs);
	CPS_CHECK_RESULT_RET_ERROR(VersionResult);

	TArray<uint8> VersionData = VersionResult.ClaimResult();

	ExportHeader.Version = *reinterpret_cast<const uint16*>(VersionData.GetData());

	TProtocolResult<TArray<uint8>> TransactionIdResult = InReader.ReceiveMessage(sizeof(ExportHeader.TransactionId), InactivityTimeoutMs);
	CPS_CHECK_RESULT_RET_ERROR(TransactionIdResult);

	TArray<uint8> TransactionIdData = TransactionIdResult.ClaimResult();
	
	ExportHeader.TransactionId = *reinterpret_cast<const uint32*>(TransactionIdData.GetData());

	return ExportHeader;
}

TProtocolResult<void> FExportHeader::Serialize(const FExportHeader& InHeader, ITcpSocketWriter& InWriter)
{
	uint16 Version = InHeader.GetVersion();
	uint32 TransactionId = InHeader.GetTransactionId();

	TArray<uint8> Data;
	Data.Append(Header.GetData(), Header.Num());
	Data.Append(reinterpret_cast<uint8*>(&Version), sizeof(Version));
	Data.Append(reinterpret_cast<uint8*>(&TransactionId), sizeof(TransactionId));

	return InWriter.SendMessage(Data);
}

uint16 FExportHeader::GetVersion() const
{
	return Version;
}

uint32 FExportHeader::GetTransactionId() const
{
	return TransactionId;
}