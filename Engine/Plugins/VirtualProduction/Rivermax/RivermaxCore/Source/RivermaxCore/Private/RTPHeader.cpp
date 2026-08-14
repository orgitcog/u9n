// Copyright Epic Games, Inc. All Rights Reserved.

#include "RTPHeader.h"

#include "RivermaxLog.h"

namespace UE::RivermaxCore::Private
{
	uint16 FRawSRD::GetRowNumber() const
	{
		return ((RowNumberHigh << 8) | RowNumberLow);
	}

	void FRawSRD::SetRowNumber(uint16 RowNumber)
	{
		RowNumberHigh = (RowNumber >> 8) & 0xFF;
		RowNumberLow = RowNumber & 0xFF;
	}

	uint16 FRawSRD::GetOffset() const
	{
		return ((OffsetHigh << 8) | OffsetLow);
	}

	void FRawSRD::SetOffset(uint16 Offset)
	{
		OffsetHigh = (Offset >> 8) & 0xFF;
		OffsetLow = Offset & 0xFF;
	}

	const uint8* GetRTPHeaderPointer(const uint8* InHeader)
	{
		check(InHeader);

		static constexpr uint32 ETH_TYPE_802_1Q = 0x8100;          /* 802.1Q VLAN Extended Header  */
		static constexpr uint32 RTP_HEADER_SIZE = 12;
		uint16* ETHProto = (uint16_t*)(InHeader + RTP_HEADER_SIZE);
		if (ETH_TYPE_802_1Q == ByteSwap(*ETHProto))
		{
			InHeader += 46; // 802 + 802.1Q + IP + UDP
		}
		else
		{
			InHeader += 42; // 802 + IP + UDP
		}
		return InHeader;
	}

	FRTPHeader::FRTPHeader(const FVideoRTPHeader& VideoRTP)
	{
		Timestamp = 0;

		if (VideoRTP.RTPHeader.Version != 2)
		{
			return;
		}

		// Pretty sure some data needs to be swapped but can't validate that until we have other hardware generating data
		SequenceNumber = (ByteSwap((uint16)VideoRTP.RTPHeader.ExtendedSequenceNumber) << 16) | ByteSwap((uint16)VideoRTP.RTPHeader.SequenceNumber);
		Timestamp = ByteSwap(VideoRTP.RTPHeader.Timestamp);
		bIsMarkerBit = VideoRTP.RTPHeader.MarkerBit;

		SyncSouceId = VideoRTP.RTPHeader.SynchronizationSource;

		SRD1.Length = ByteSwap((uint16)VideoRTP.SRD1.Length);
		SRD1.DataOffset = VideoRTP.SRD1.GetOffset();
		SRD1.RowNumber = VideoRTP.SRD1.GetRowNumber();
		SRD1.bIsFieldOne = VideoRTP.SRD1.FieldIdentification;
		SRD1.bHasContinuation = VideoRTP.SRD1.ContinuationBit;

		if (SRD1.bHasContinuation)
		{
			SRD2.Length = ByteSwap((uint16)VideoRTP.SRD2.Length);
			SRD2.DataOffset = VideoRTP.SRD2.GetOffset();
			SRD2.RowNumber = VideoRTP.SRD2.GetRowNumber();
			SRD2.bIsFieldOne = VideoRTP.SRD2.FieldIdentification;
			SRD2.bHasContinuation = VideoRTP.SRD2.ContinuationBit;

			if (SRD2.bHasContinuation == true)
			{
				UE_LOG(LogRivermax, Verbose, TEXT("Received SRD with more than 2 SRD which isn't supported."));
			}
		}
	}

	uint16 FRTPHeader::GetTotalPayloadSize() const
	{
		uint16 PayloadSize = SRD1.Length;
		if (SRD1.bHasContinuation)
		{
			PayloadSize += SRD2.Length;
		}

		return PayloadSize;
	}

	uint16 FRTPHeader::GetLastPayloadSize() const
	{
		if (SRD1.bHasContinuation)
		{
			return SRD2.Length;
		}

		return SRD1.Length;
	}

	uint16 FRTPHeader::GetLastRowOffset() const
	{
		if (SRD1.bHasContinuation)
		{
			return SRD2.DataOffset;
		}

		return SRD1.DataOffset;
	}

	uint16 FRTPHeader::GetLastRowNumber() const
	{
		if (SRD1.bHasContinuation)
		{
			return SRD2.RowNumber;
		}

		return SRD1.RowNumber;
	}

	uint8 FAncillaryTimecodeHeaderFields::EvenParity(uint64 Num)
	{
		return (uint8)(FMath::CountBits(Num) & 0x1);
	}

	void FAncillaryTimecodeHeaderFields::SetATCTimecode(uint8 Hours, uint8 Minutes, uint8 Seconds, uint8 Frames, bool bDropFrame)
	{
		auto ConvertToBinaryCodedDecimal = [](uint8 OriginalNum) -> uint8 { return ((OriginalNum / 10) << 4) | (OriginalNum % 10); };

		auto ParityWrap = [this](uint8 v) -> uint16
			{
				uint8 p = EvenParity(v);
				return (v & 0xFF) | ((p ? 1 : 0) << 8) | ((p ? 0 : 1) << 9);
			};

		UserData1 = ParityWrap(ConvertToBinaryCodedDecimal(Frames) | (bDropFrame ? 0x40 : 0x00));  // Bit 7 = DF
		UserData2 = ParityWrap(ConvertToBinaryCodedDecimal(Seconds));                              // Bit 6 = BGF1 if needed
		UserData3 = ParityWrap(ConvertToBinaryCodedDecimal(Minutes));                              // Bit 6 = BGF2 if needed
		UserData4 = ParityWrap(ConvertToBinaryCodedDecimal(Hours));                                // Bit 6 = BGF3 if needed
		UserData5 = ParityWrap(0x00); // User bits 1
		UserData6 = ParityWrap(0x00); // User bits 2
		UserData7 = ParityWrap(0x00); // User bits 3
		UserData8 = ParityWrap(0x00); // User bits 4

		uint16 DID = 0x60;
		uint16 SDID = 0x60;
		uint16 DataCount = 8;

		SetDataCount(DataCount); // Also sets parity/inverse parity bits

		// Compute checksum
		uint16 CheckSum = (DID & 0x1FF) + (SDID & 0x1FF) + (DataCount & 0x1FF)
			+ (UserData1 & 0x1FF) + (UserData2 & 0x1FF) + (UserData3 & 0x1FF) + (UserData4 & 0x1FF)
			+ (UserData5 & 0x1FF) + (UserData6 & 0x1FF) + (UserData7 & 0x1FF) + (UserData8 & 0x1FF);

		CheckSum &= 0x1FF;

		uint8 Lower8Bits = CheckSum & 0xFF;
		uint8 NumOfSetBits = FMath::CountBits(Lower8Bits);
		uint8 ParityBit = (NumOfSetBits % 2 == 0) ? 0 : 1;
		uint8 InverseParityBit = ParityBit ^ 1;

		CheckSum |= (ParityBit << 8);
		CheckSum |= (InverseParityBit << 9);

		ChecksumHigh = (CheckSum >> 2) & 0xFF;
		ChecksumLow = CheckSum & 0xFF;

	}
}






