// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxOutAncStream.h"

#include "Async/Async.h"
#include "CudaModule.h"
#include "IRivermaxCoreModule.h"
#include "IRivermaxBoundaryMonitor.h"
#include "IRivermaxManager.h"
#include "Misc/ByteSwap.h"
#include "RivermaxFrameAllocator.h"
#include "RivermaxFrameManager.h"
#include "RivermaxLog.h"
#include "RivermaxPTPUtils.h"
#include "RivermaxTracingUtils.h"
#include "RivermaxTypes.h"
#include "RivermaxUtils.h"



#define GET_FRAME_INDEX(OUTPUT_FRAME) OUTPUT_FRAME->GetFrameCounter() % Options.NumberOfBuffers
namespace UE::RivermaxCore::Private
{
	// Fills the Rivermax memory with ANC header and data.
	void FillChunk(const TSharedPtr<FRivermaxAncOutputOptions> InStreamOptions, const FRivermaxOutputStreamMemory& InStreamMemory, uint8* InFirstPacketStartPointer, uint16* InPayloadSizes, const UE::RivermaxCore::Private::FRivermaxOutputStreamData& StreamData, TSharedPtr<UE::RivermaxCore::Private::FRivermaxOutputFrame> CurrentFrame)
	{
		constexpr uint8 PayloadType = 97;

		// Anc field values.
		constexpr uint8 ProgressiveField = 0b00;
		constexpr uint8 InterlaceField = 0b10;
		constexpr uint8 InterlaceSecondField = 0b11;

		for (size_t PayloadIndex = 0; PayloadIndex < InStreamMemory.PacketsPerChunk; ++PayloadIndex, InFirstPacketStartPointer += InStreamMemory.PacketsPerChunk) {
			memset(InFirstPacketStartPointer, 0, InStreamMemory.PayloadSize);
			FAncRTPHeader* AncRtpHeader = (struct FAncRTPHeader*)&InFirstPacketStartPointer[0];
			//RTP header initialization
			AncRtpHeader->RTPHeader.Version = 2;
			AncRtpHeader->RTPHeader.ExtensionBit = 0;
			AncRtpHeader->RTPHeader.PaddingBit = 0;
			AncRtpHeader->RTPHeader.MarkerBit = 1;
			AncRtpHeader->RTPHeader.PayloadType = PayloadType;
			AncRtpHeader->RTPHeader.SequenceNumber = ByteSwap((uint16)(StreamData.SequenceNumber & 0xFFFF));
			AncRtpHeader->RTPHeader.Timestamp = ByteSwap(CurrentFrame->MediaTimestamp);
			AncRtpHeader->RTPHeader.SynchronizationSource = StreamData.SynchronizationSource;
			AncRtpHeader->RTPHeader.ExtendedSequenceNumber = ByteSwap((uint16)((StreamData.SequenceNumber >> 16) & 0xFFFF));

			AncRtpHeader->ANCCount = 1;
			AncRtpHeader->Field = ProgressiveField;

			FAncillaryTimecodeHeaderFields* AncDataHeader = (struct FAncillaryTimecodeHeaderFields*)&InFirstPacketStartPointer[sizeof(FAncRTPHeader)];
			AncDataHeader->DataPacketHeaderFields.ColorDiff = 0;
			AncDataHeader->DataPacketHeaderFields.SetLineNumber(0x7FF);
			AncDataHeader->DataPacketHeaderFields.SetHorizontalOffset(0xFFF);
			AncDataHeader->DataPacketHeaderFields.StreamFlag = 0;
			AncDataHeader->DataPacketHeaderFields.StreamNum = 0;
			AncDataHeader->SetDID(InStreamOptions->DID);
			AncDataHeader->SetSDID(InStreamOptions->SDID);
			uint16 Length = sizeof(FAncillaryTimecodeHeaderFields);
			AncDataHeader->SetATCTimecode(CurrentFrame->Timecode.Hours, CurrentFrame->Timecode.Minutes, CurrentFrame->Timecode.Seconds, CurrentFrame->Timecode.Frames, CurrentFrame->Timecode.bDropFrameFormat);
			AncRtpHeader->Length = ByteSwap((uint16)(Length));
			InPayloadSizes[PayloadIndex] = sizeof(FAncRTPHeader) /*RTP + ERTP headers*/ + Length;
		}
	}


	FRivermaxOutAncStream::FRivermaxOutAncStream(const TArray<char>& SDPDescription)
		: FRivermaxOutStream(SDPDescription)
		, FrameInfoToSend(MakeShared<FRivermaxOutputAncInfo>())
	{
		StreamType = ERivermaxStreamType::ANC_2110_40_STREAM;
	}

	bool FRivermaxOutAncStream::PushFrame(TSharedPtr<IRivermaxOutputInfo> FrameInfo)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutAncStream::PushVideoFrame);

		// Clear reserved frame if there is one. If not, try to get free frame.
		// ReservedFrame should always be valid when block on reservation mode is used.
		TSharedPtr<FRivermaxOutputFrame> ReservedFrame;

		// At the moment we always have reserved frame for ANC
		check(ReservedFrames.RemoveAndCopyValue(FrameInfo->FrameIdentifier, ReservedFrame))

		// If this is invalid it means that frame locking mode is BlockOnReservation and the render ran faster than media output fps.
		FrameInfoToSend->FrameIdentifier = FrameInfo->FrameIdentifier;
		FrameInfoToSend->Height = FrameInfo->Height;
		FrameInfoToSend->Width = FrameInfo->Width;
		FrameInfoToSend->Stride = FrameInfo->Stride;
		FrameReadyToSendSignal->Trigger();
		return true;
	}

	bool FRivermaxOutAncStream::InitializeStreamMemoryConfig()
	{
		using namespace UE::RivermaxCore::Private::Utils;

		constexpr int64 PacketsPerFrame = 1;

		// We might need a smaller packet to complete the end of frame so ceil to the next value
		StreamMemory.PacketsPerFrame = PacketsPerFrame;

		constexpr size_t StridesInChunk = 1;
		StreamMemory.PacketsPerChunk = StridesInChunk;
		StreamMemory.FramesFieldPerMemoryBlock = CachedCVars.bUseSingleMemblock ? Options.NumberOfBuffers : 1;

		// Only one chunk with one packet.
		constexpr int64 ChunksPerFrame = 1.;
		StreamMemory.ChunksPerFrameField = ChunksPerFrame;
		const uint64 RealPacketsPerFrame = StreamMemory.ChunksPerFrameField * StreamMemory.PacketsPerChunk;
		StreamMemory.PacketsPerMemoryBlock = RealPacketsPerFrame * StreamMemory.FramesFieldPerMemoryBlock;
		StreamMemory.ChunksPerMemoryBlock = StreamMemory.FramesFieldPerMemoryBlock * StreamMemory.ChunksPerFrameField;
		StreamMemory.MemoryBlockCount = Options.NumberOfBuffers / StreamMemory.FramesFieldPerMemoryBlock;
		StreamMemory.DataBlockID = 0;
		// A size close to max UDP packet size. Since it is only one packet.
		StreamMemory.PayloadSize = 1300;

		StreamMemory.ChunkSpacingBetweenMemcopies = 1;
		StreamMemory.bUseIntermediateBuffer = true;

		if (!SetupFrameManagement())
		{
			return false;
		}

		StreamMemory.MemoryBlocks.SetNumZeroed(StreamMemory.MemoryBlockCount);
		CachedAPI->rmx_output_media_init_mem_blocks(StreamMemory.MemoryBlocks.GetData(), StreamMemory.MemoryBlockCount);
		for (uint32 BlockIndex = 0; BlockIndex < StreamMemory.MemoryBlockCount; ++BlockIndex)
		{
			rmx_output_media_mem_block& Block = StreamMemory.MemoryBlocks[BlockIndex];
			CachedAPI->rmx_output_media_set_chunk_count(&Block, StreamMemory.ChunksPerMemoryBlock);

			// Anc only needs one sub block since we don't split header and data.
			constexpr uint8 SubBlockCount = 1;
			CachedAPI->rmx_output_media_set_sub_block_count(&Block, SubBlockCount);
		}
		
		return true;
	}

	bool FRivermaxOutAncStream::IsFrameAvailableToSend()
	{
		// This should also depend on Video stream.
		return true;
	}

	bool FRivermaxOutAncStream::CopyFrameData(const TSharedPtr<FRivermaxOutputFrame>& SourceFrame, uint8* DestinationBase)
	{
		TSharedPtr<FRivermaxAncOutputOptions> StreamOptions = StaticCastSharedPtr<FRivermaxAncOutputOptions>(Options.StreamOptions[(uint8)StreamType]);

		// Alternative to rmx_output_media_set_packet_layout. Required for dynamically sized packets.
		CachedAPI->rmx_output_media_set_chunk_packet_count(&StreamData.ChunkHandle, (size_t)StreamMemory.PacketsPerChunk);

		// This size will be filled with the actual sizes of the packets.
		uint16* PayloadSizes = rmx_output_media_get_chunk_packet_sizes(&StreamData.ChunkHandle, StreamMemory.DataBlockID);

		FillChunk(StreamOptions, StreamMemory, reinterpret_cast<uint8*>(CurrentFrame->FrameStartPtr), PayloadSizes, StreamData, CurrentFrame);

		OnFrameReadyToBeSent();

		return true;
	}

	bool FRivermaxOutAncStream::SetupFrameManagement()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutAncStream::SetupFrameManagement);

		return true;
	}

	void FRivermaxOutAncStream::CleanupFrameManagement()
	{
	}

	bool FRivermaxOutAncStream::ReserveFrame(uint64 FrameCounter) const
	{
		// There is only one reserved frame at the time per stream.
		TSharedPtr<FRivermaxOutputFrame> ReservedFrame = MakeShared<FRivermaxOutputFrame>();

		if (ReservedFrame.IsValid())
		{
			ReservedFrame->SetFrameCounter(FrameCounter);
			ReservedFrames.Add(FrameCounter, ReservedFrame);
		}

		return ReservedFrame.IsValid();
	}

	TSharedPtr<FRivermaxOutputFrame> FRivermaxOutAncStream::GetNextFrameToSend(bool bWait)
	{
		TSharedPtr<FRivermaxOutputFrame> NextFrameToSend = MakeShared<FRivermaxOutputFrame>();

		if (bWait)
		{
			while (!NextFrameToSend.IsValid() && bIsActive)
			{
				FrameReadyToSendSignal->Wait();
				//NextFrameToSend = FrameManager->DequeueFrameToSend();
			}
		}
		return NextFrameToSend;
	}

	void FRivermaxOutAncStream::LogStreamDescriptionOnCreation() const
	{
		FRivermaxOutStream::LogStreamDescriptionOnCreation();

		TStringBuilder<512> StreamDescription;

		TSharedPtr<FRivermaxAncOutputOptions> StreamOptions = StaticCastSharedPtr<FRivermaxAncOutputOptions>(Options.StreamOptions[(uint8)StreamType]);
		StreamDescription.Appendf(TEXT("FrameRate = %s, "), *StreamOptions->FrameRate.ToPrettyText().ToString());
		StreamDescription.Appendf(TEXT("Alignment = %s, "), LexToString(Options.AlignmentMode));
		StreamDescription.Appendf(TEXT("Framelocking = %s."), LexToString(Options.FrameLockingMode));

		UE_LOG(LogRivermax, Display, TEXT("%s"), *FString(StreamDescription));
	}

	void FRivermaxOutAncStream::SetupRTPHeaders()
	{
		// ANC RTP headers are shipped with the payload. All needs to be done is incrementation of packets and sequence number.
		++StreamData.SequenceNumber;
	}

	void FRivermaxOutAncStream::CompleteCurrentFrame(bool bReleaseFrame)
	{
		FRivermaxOutStream::CompleteCurrentFrame(bReleaseFrame);
	}
}

