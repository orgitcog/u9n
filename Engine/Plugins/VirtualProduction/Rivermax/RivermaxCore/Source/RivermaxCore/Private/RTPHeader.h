// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"


namespace UE::RivermaxCore::Private
{
	//RTP Header used for 2110 following https://www.rfc-editor.org/rfc/rfc4175.html

	/* RTP Header -  12 bytes
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	| V |P|X|  CC   |M|     PT      |            SEQ                |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                           timestamp                           |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                           ssrc                                |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|    Extended Sequence Number   |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/

	/** Raw representation as it's built for the network */

/** @note When other platform than windows are supports, reverify support for pragma_pack and endianness */
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(push, 1)
#endif
	/** Total size should be 14 octets. */
	struct FRawRTPHeader
	{
		uint8 ContributingSourceCount   : 4;
		uint8 ExtensionBit              : 1;
		uint8 PaddingBit                : 1;
		uint8 Version                   : 2;
		uint8 PayloadType               : 7;
		uint8 MarkerBit                 : 1;
		uint16 SequenceNumber           : 16;
		uint32 Timestamp                : 32;
		uint32 SynchronizationSource    : 32;
		uint16 ExtendedSequenceNumber   : 16;

		/** The base value is 0x0eb51dbf which will be used for video. Anc is VideoSyncSource + 1. */
		static constexpr uint32 VideoSynchronizationSource = 0x0eb51dbf;
	};

	/** 
	SRD Header. Total packed size should be 6 octets.

	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|           SRD Length          |F|     SRD Row Number          |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|C|         SRD Offset          |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	struct FRawSRD
	{
		uint16 Length : 16;

		uint8 RowNumberHigh : 7;

		uint8 FieldIdentification : 1;

		uint8 RowNumberLow : 8;

		uint8 OffsetHigh : 7;

		/** If set indicates that there is another SRD following this one.*/
		uint8 ContinuationBit : 1;
		uint8 OffsetLow : 8;


		/** Returns SRD associated row number */
		uint16 GetRowNumber() const;

		/** Sets SRD associated row number */
		void SetRowNumber(uint16 RowNumber);

		/** Returns SRD pixel offset in its associated row */
		uint16 GetOffset() const;

		/** Sets SRD pixel offset in its associated row */
		void SetOffset(uint16 Offset);
	};

	/** Total size should be 26 octets. */
	struct FVideoRTPHeader
	{
		FRawRTPHeader RTPHeader;

		FRawSRD SRD1;                   // 48

		FRawSRD SRD2;                   // 48

		/** Size of RTP representation whether it has one or two SRDs */
		static constexpr uint32 OneSRDSize = 20;
		static constexpr uint32 TwoSRDSize = 26;
	};

	/*
	* https://datatracker.ietf.org/doc/html/rfc8331
	*
	*       0                   1                   2                   3
	*       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	*       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*       |           Length=32           | ANC_Count=2   | F | reserved  |
	*       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*       |    reserved                   |
	*/
	struct FAncRTPHeader 
	{
		FRawRTPHeader RTPHeader;

		/**
		* From RFC 8331:
		*  Number of octets of the ANC data RTP payload, beginning with 
		*  the "C" bit of the first ANC packet data header, as an
        *  unsigned integer in network byte order.  Note that all
        *  word_align fields contribute to the calculation of the Length
        *  field.
		*/
		uint16 Length         : 16;
		uint8 ANCCount        : 8;
		uint8 ReservedHigh    : 6;

		/**
		*	These two bits relate to signaling the field specified by the
        *   RTP timestamp in an interlaced SDI raster.  A value of 0b00
        *   indicates that either the video format is progressive or that
        *   no field is specified.
		*/
		uint8 Field           : 2;
		uint16 ReservedLow    : 16;
	};

	/**
	* For FAncillaryDataHeaderFields and FAncillaryTimecodeHeaderFields
	*       0                   1                   2                   3
	*       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	*       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*       |C|   Line_Number=9     |   Horizontal_Offset   |S| StreamNum=0 |
	*       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*       |         DID       |        SDID       |  Data_Count=0x84  |
	*       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*                                User_Data_Words...
	*       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*                   |   Checksum_Word   |         word_align            |
	*       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	struct FAncillaryDataHeaderFields 
	{
		/**
		*   +-------------+--------------------------------------------------------+
		*	| Line_Number | ANC data packet generic vertical location              |
		*	+-------------+--------------------------------------------------------+
		*	|   0x7FF     | Without specific line location within the field or     |
		*	|             | frame                                                  |
		*/
		uint8 LineNumberHigh : 7;

		uint8 ColorDiff : 1;

		/**
		* ANC data packet generic horizontal location
		* 0xFFF     | Without specific horizontal location   
		*/
		uint8 HorizontalOffsetHigh : 4;

		uint8 LineNumberLow : 4;

		uint8 HorizontalOffsetLow : 8;

		uint8 StreamNum : 7;

		/**
		* Data Stream Flag.
		* This field indicates whether the data stream number of a multi-stream data mapping used to
		* transport the ANC data packet is specified.
		*/
		uint8 StreamFlag : 1;

		inline void SetLineNumber(uint16 InLineNumber)
		{
			LineNumberLow = (0x000F & InLineNumber);
			LineNumberHigh = (0x007F & (InLineNumber >> 4));
		}

		inline void SetHorizontalOffset(uint16 InHorizontalOffset)
		{
			HorizontalOffsetLow = (0x00FF & InHorizontalOffset);
			HorizontalOffsetHigh = (0x000F & (InHorizontalOffset >> 8));
		}
		

	};


	/**
	* This will contain ancillary timecode data packet starting from DID field.
	*/
	struct FAncillaryTimecodeHeaderFields
	{
		FAncillaryDataHeaderFields DataPacketHeaderFields;

		// More on DID and SDID can be found here: https://smpte-ra.org/smpte-ancillary-data-smpte-st-291/
		uint8 DIDHigh : 8;
		uint8 SDIDHigh : 6;
		uint8 DIDLow : 2;

		/**
		* From: https://datatracker.ietf.org/doc/html/rfc8331
		* Data_Count: 10 bits The lower 8 bits of Data_Count, corresponding to bits b7
		* (MSB; most significant bit) through b0 (LSB; least significant bit) of the 10-bit Data_Count word,
		* contain the actual count of 10-bit words in User_Data_Words.  Bit b8 is the even parity for
		* bits b7 through b0, and bit b9 is the inverse (logical NOT) of bit b8.
		*/
		uint8 DataCountHigh : 2;
		uint8 DataCountEvenParity : 1;
		uint8 DataCountInverseLogical : 1;
		uint8 SDIDLow : 4;

		uint8 DataCountLow : 6;
		
		uint32 UserData1 : 10;
		uint32 UserData2 : 10;
		uint32 UserData3 : 10;
		uint32 UserData4 : 10;
		uint32 UserData5 : 10;
		uint32 UserData6 : 10;
		uint32 UserData7 : 10;
		uint32 UserData8 : 10;

		uint8 ChecksumHigh : 2;

		uint8 ChecksumLow : 8;

		// Word align 1st to 6th bit 6 bits
		uint8 WordAlign6thBit : 8;

		/** Checks if the number of set bits is odd or even. Returns 0 if the number of 1s is even. 1 if odd. */
		uint8 EvenParity(uint64 Num);

		/** Add even parity check to the provided value. */
		inline uint16 DidSdidAddParity(uint16 Value)
		{
			Value &= 0xff;
			uint8 Parity = EvenParity(Value);

			Value |= ((!Parity << 1) | Parity) << 8;
			return Value;
		}

		/** Set DID value for this ANC header. */
		inline void SetDID(uint16 InDID)
		{
			InDID = DidSdidAddParity(InDID);
			DIDLow = (0x0003 & InDID);
			DIDHigh = (0x00FF & (InDID >> 2));
		}

		/** Get DID value for this ANC header. */
		inline uint16 GetDID()
		{
			return (((uint16)DIDLow)) | (((uint16)DIDHigh) << 2);
		}

		/** Set SDID value for this ANC header. */
		inline void SetSDID(uint16 InSDID)
		{
			InSDID = DidSdidAddParity(InSDID);
			SDIDLow = (0x000F & InSDID);
			SDIDHigh = (0x003F & (InSDID >> 4));
		}

		/** Get SDID value for this ANC header. */
		inline uint16 GetSDID()
		{
			return (((uint16)SDIDLow)) | (((uint16)SDIDHigh) << 4);
		}

		/** Set Data count. */
		inline void SetDataCount(uint8 RawDataCount)
		{
			DataCountLow = (0x3F & (uint16)RawDataCount);
			DataCountHigh = (0x03 & (RawDataCount >> 6));
			DataCountEvenParity = EvenParity(GetDataCount());
			DataCountInverseLogical = !DataCountEvenParity;
		}

		/** Get Data count. */
		inline uint16 GetDataCount()
		{
			return (((uint16)DataCountLow)) | (((uint16)DataCountHigh) << 6);
		}

		/** Sets the ANC timecode. */
		void SetATCTimecode(uint8 Hours, uint8 Minutes, uint8 Seconds, uint8 Frames, bool bDropFrame);
	};
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

	struct FSRDHeader
	{
		/** Length of payload. Is a multiple of pgroup (see pixel formats) */
		uint16 Length = 0;

		/** False if progressive or first field of interlace. True if second field of interlace */
		bool bIsFieldOne = false;

		/** Video line number, starts at 0 */
		uint16 RowNumber = 0;

		/** Whether another SRD is following this one */
		bool bHasContinuation = false;

		/** Location of the first pixel in payload, in pixel */
		uint16 DataOffset = 0;
	};

	/** RTP header built from network representation not requiring any byte swapping */
	struct FRTPHeader
	{
		FRTPHeader() = default;
		FRTPHeader(const FVideoRTPHeader& VideoRTP);

		/** Returns the total payload of this RTP */
		uint16 GetTotalPayloadSize() const;
		
		/** Returns the payload size of the last SRD in this RTP */
		uint16 GetLastPayloadSize() const;

		/** Returns the row offset of the last SRD in this RTP */
		uint16 GetLastRowOffset() const;

		/** Returns the row number of the last SRD in this RTP */
		uint16 GetLastRowNumber() const;

		/** Sequence number including extension if present */
		uint32 SequenceNumber = 0;

		/** Timestamp of frame in the specified clock resolution. Video is typically 90kHz */
		uint32 Timestamp = 0;

		/** Identification of this stream */
		uint32 SyncSouceId = 0;

		/** Whether extensions (SRD headers) are present */
		bool bHasExtension = false;

		/** True if RTP packet is last of video stream */
		bool bIsMarkerBit = false;

		/** Only supports 2 SRD for now. Adjust if needed */
		FSRDHeader SRD1;
		FSRDHeader SRD2;
	};

	/** Returns RTPHeader pointer from a raw ethernet packet skipping 802, IP, UDP headers */
	const uint8* GetRTPHeaderPointer(const uint8* InHeader);
}


