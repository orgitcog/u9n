// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxOutVideoStream.h"

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
	static TAutoConsoleVariable<int32> CVarRivermaxOutputEnableMultiSRD(
		TEXT("Rivermax.Output.EnableMultiSRD"), 0,
		TEXT("When enabled and if the row cannot be split evenly, non-uniform payloads will be used. The last packet for the frame will not be fully filled with data.\n"
			"If disabled, the payloads will be split evenly or the 2110 stream will be disabled."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputLinesPerChunk(
		TEXT("Rivermax.Output.LinesPerChunk"), 4,
		TEXT("Defines the number of lines to pack in a chunk. Higher number will increase latency"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputMaximizePacketSize(
		TEXT("Rivermax.Output.MaximizePacketSize"), 1,
		TEXT("Enables bigger packet sizes to maximize utilisation of potential UDP packet. If not enabled, packet size will be aligned with HD/4k sizes"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputMaxFrameMemorySliceCount(
		TEXT("Rivermax.Output.FrameSliceCount"), 30,
		TEXT("Max number of memcopies done per frame when using intermediate buffer. As frame gets bigger, we can't do a single memcopy or timings will be broken. Can be smaller in order to fit inside chunk count."),
		ECVF_Default);

	static TAutoConsoleVariable<bool> CVarRivermaxOutputEnableIntermediateBuffer(
		TEXT("Rivermax.Output.Alignment.EnableIntermediateBuffer"), true,
		TEXT("Uses an intermediate buffer used by Rivermax when sending data out.\n")
		TEXT("During scheduling, captured frame data will be copied over intermediate buffer.\n")
		TEXT("Only applies to alignment points scheduling mode."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputMemcopyChunkSpacing(
		TEXT("Rivermax.Output.Scheduling.MemcopyChunkSpacing"), 10,
		TEXT("Number of chunks between each memcopy to help with timing for different frame format."),
		ECVF_Default);

	// TODO: To be removed or improved in 5.7. In 5.6.1 Media Capture will be responsible for padding the resolution in case row cannot be split into equal sized packets due to issues with Multi-srd.
	bool FindPayloadSize(uint32 InBytesPerLine, const uint32 PixelGroupSize, uint16& OutPayloadSize)
	{
		/** Maximum payload size in bytes that the plugin can send based on UDP max size and RTP header.  */
		static constexpr uint32 MaxPayloadSize = 1420;

		/** Smallest payload size (bytes) to use as a lower bound in search for a payload that can be equal across a line */
		static constexpr uint32 MinPayloadSize = 600;

		// For now, we only find a payload size that can be equal across one line
		// Support for last payload of a line being smaller is there but is causing issue
		// We fail to output if we receive a resolution for which we can't find an equal payload size
		int32 TestPoint = InBytesPerLine / MaxPayloadSize;
		if (TestPoint == 0)
		{
			if (InBytesPerLine > MinPayloadSize)
			{
				if (InBytesPerLine % PixelGroupSize == 0)
				{
					OutPayloadSize = InBytesPerLine;
					return true;
				}
			}
			return false;
		}

		while (true)
		{
			const int32 TestSize = InBytesPerLine / TestPoint;
			if (TestSize < MinPayloadSize)
			{
				break;
			}

			if (TestSize <= MaxPayloadSize)
			{
				if ((TestSize % PixelGroupSize) == 0 && (InBytesPerLine % TestPoint) == 0)
				{
					OutPayloadSize = TestSize;
					return true;
				}
			}

			++TestPoint;
		}

		return false;
	}

	void UpdateVideoRtpHeader
		( FVideoRTPHeader& OutVideoHeader
		, uint32& OutPacketCounter
		, uint16& OutSRDOffset
		, uint32& OutLineNumber
		, uint32 InSequenceNumber
		, uint32 InTimeStamp
		, const FIntPoint& AlignedResolution
		, const FVideoFormatInfo& InFormatInfo
		, const FRivermaxOutputStreamMemory& InStreamMemory
	)
	{
		OutVideoHeader = {};
		OutVideoHeader.RTPHeader = {};
		OutVideoHeader.RTPHeader.Version = 2;
		OutVideoHeader.RTPHeader.PaddingBit = 0;
		OutVideoHeader.RTPHeader.ExtensionBit = 0;
		OutVideoHeader.RTPHeader.PayloadType = 96; //Payload type should probably be inferred from SDP

		OutVideoHeader.RTPHeader.SequenceNumber = ByteSwap((uint16)(InSequenceNumber & 0xFFFF));
		OutVideoHeader.RTPHeader.Timestamp = ByteSwap(InTimeStamp);
		OutVideoHeader.RTPHeader.ExtendedSequenceNumber = ByteSwap((uint16)((InSequenceNumber >> 16) & 0xFFFF));

		//2110 specific header
		OutVideoHeader.RTPHeader.SynchronizationSource = ByteSwap((uint32)0x0eb51dbd);  // Should Unreal has its own synch source ID

		{
			if (OutPacketCounter + 1 == InStreamMemory.PacketsPerFrame)
			{
				OutVideoHeader.RTPHeader.MarkerBit = 1; // last packet in frame (Marker)
			}

			// Verify if payload size exceeds line 
			const uint32 CurrentPayloadSize = InStreamMemory.PayloadSizes[OutPacketCounter];

			const uint32 LineSizeOffset = ((OutSRDOffset / InFormatInfo.PixelGroupCoverage) * InFormatInfo.PixelGroupSize);
			const uint32 LineSize = ((AlignedResolution.X / InFormatInfo.PixelGroupCoverage) * InFormatInfo.PixelGroupSize);

			const uint16 SRD1Length = FMath::Min(LineSize - LineSizeOffset, CurrentPayloadSize);
			const uint16 SRD1PixelCount = SRD1Length / InFormatInfo.PixelGroupSize * InFormatInfo.PixelGroupCoverage;
			uint16 SRD2Length = SRD1Length < CurrentPayloadSize ? CurrentPayloadSize - SRD1Length : 0;
			if (SRD2Length && OutLineNumber == ((uint32)AlignedResolution.Y - 1))
			{
				SRD2Length = 0;
			}

			OutVideoHeader.SRD1.Length = ByteSwap(SRD1Length);
			OutVideoHeader.SRD1.SetRowNumber(OutLineNumber); //todo divide by 2 if interlaced
			OutVideoHeader.SRD1.SetOffset(OutSRDOffset);
			OutVideoHeader.SRD1.ContinuationBit = SRD2Length > 0;
			OutVideoHeader.SRD1.FieldIdentification = 0; //todo when fields are sent for interlace

			OutSRDOffset += SRD1PixelCount;
			if (OutSRDOffset >= AlignedResolution.X)
			{
				OutSRDOffset = 0;
				++OutLineNumber;
			}

			if (SRD2Length > 0)
			{
				OutVideoHeader.SRD2.Length = ByteSwap(SRD2Length);
				OutVideoHeader.SRD2.SetRowNumber(OutLineNumber);
				OutVideoHeader.SRD2.SetOffset(OutSRDOffset);
				OutVideoHeader.SRD2.ContinuationBit = 0;
				OutVideoHeader.SRD2.FieldIdentification = 0;

				const uint16 SRD2PixelCount = SRD2Length / InFormatInfo.PixelGroupSize * InFormatInfo.PixelGroupCoverage;
				OutSRDOffset += SRD2PixelCount;
				if (OutSRDOffset >= AlignedResolution.X)
				{
					OutSRDOffset = 0;
					++OutLineNumber;
				}
			}
		}
	}

	uint32 FindLinesPerChunk(const FRivermaxOutputOptions& InOptions)
	{
		// More lines per chunks mean we will do more work prior to start sending a chunk. So, added 'latency' in terms of packet / parts of frame.
		// Less lines per chunk mean that sender thread might starve.
		// SDK sample uses 4 lines for UHD and 8 for HD. 
		return CVarRivermaxOutputLinesPerChunk.GetValueOnAnyThread();
	}

	uint16 GetPayloadSize(ESamplingType SamplingType)
	{
		const FVideoFormatInfo FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(SamplingType);
		uint16 PayloadSize;
		switch (SamplingType)
		{

		case ESamplingType::YUV444_10bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_10bit:
		{
			PayloadSize = 1200;
			break;
		}
		case ESamplingType::YUV444_8bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_8bit:
		{
			PayloadSize = 1152;
			break;
		}
		case ESamplingType::YUV444_12bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_12bit:
		{
			PayloadSize = 1152;
			break;
		}
		case ESamplingType::YUV444_16bit:
		{
			// Passthrough
		}
		case ESamplingType::YUV444_16bitFloat:
		{
			// Passthrough
		}
		case ESamplingType::RGB_16bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_16bitFloat:
		{
			PayloadSize = 1152;
			break;
		}
		case ESamplingType::YUV422_8bit:
		{
			PayloadSize = 1280;
			break;
		}
		case ESamplingType::YUV422_10bit:
		{
			PayloadSize = 1200;
			break;
		}
		case ESamplingType::YUV422_12bit:
		{
			PayloadSize = 1152;
			break;
		}
		case ESamplingType::YUV422_16bit:
		{
			// Passthrough
		}
		case ESamplingType::YUV422_16bitFloat:
		{
			PayloadSize = 1280;
			break;
		}
		default:
		{
			checkNoEntry();
			PayloadSize = 1200;
			break;
		}
		}

		ensure(PayloadSize % FormatInfo.PixelGroupSize == 0);
		return PayloadSize;
	}

	/**
	 * Returns a payload closer to the max value we can have for standard UDP size
	 * RTPHeader can be bigger depending on configuration so we'll cap payload at 1400.
	 */
	uint16 GetMaximizedPayloadSize(ESamplingType SamplingType)
	{
		const FVideoFormatInfo FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(SamplingType);
		uint16 PayloadSize;
		switch (SamplingType)
		{

		case ESamplingType::YUV444_10bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_10bit:
		{
			PayloadSize = 1395;
			break;
		}
		case ESamplingType::YUV444_8bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_8bit:
		{
			PayloadSize = 1398;
			break;
		}
		case ESamplingType::YUV444_12bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_12bit:
		{
			PayloadSize = 1395;
			break;
		}
		case ESamplingType::YUV444_16bit:
		{
			// Passthrough
		}
		case ESamplingType::YUV444_16bitFloat:
		{
			// Passthrough
		}
		case ESamplingType::RGB_16bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_16bitFloat:
		{
			PayloadSize = 1398;
			break;
		}
		case ESamplingType::YUV422_8bit:
		{
			PayloadSize = 1400;
			break;
		}
		case ESamplingType::YUV422_10bit:
		{
			PayloadSize = 1400;
			break;
		}
		case ESamplingType::YUV422_12bit:
		{
			PayloadSize = 1398;
			break;
		}
		case ESamplingType::YUV422_16bit:
		{
			// Passthrough
		}
		case ESamplingType::YUV422_16bitFloat:
		{
			PayloadSize = 1400;
			break;
		}
		default:
		{
			checkNoEntry();
			PayloadSize = 1200;
			break;
		}
		}

		ensure(PayloadSize % FormatInfo.PixelGroupSize == 0);
		return PayloadSize;
	}


	FRivermaxOutVideoStream::FRivermaxOutVideoStream(const TArray<char>& SDPDescription)
		: FRivermaxOutStream(SDPDescription)
	{
	}

	struct FRTPHeaderPrefiller
	{
		FRTPHeaderPrefiller(FRivermaxOutVideoStream& InRmaxOutputStream)
			: Stream(InRmaxOutputStream)
		{
			RunningSRDOffsetPerFrame.SetNumZeroed(Stream.Options.NumberOfBuffers);
			RunningLineNumberPerFrame.SetNumZeroed(Stream.Options.NumberOfBuffers);
		}

		void Update(uint32 PacketIndex)
		{
			using namespace UE::RivermaxCore::Private::Utils;

			const int32 PacketCount = Stream.StreamMemory.ChunksPerFrameField * Stream.StreamMemory.PacketsPerChunk;
			uint32 BufferIndex = 0;
			for (int32 MemblockIndex = 0; MemblockIndex < Stream.StreamMemory.RTPHeaders.Num(); ++MemblockIndex)
			{
				for (uint32 FrameInBlockIndex = 0; FrameInBlockIndex < Stream.StreamMemory.FramesFieldPerMemoryBlock; ++FrameInBlockIndex)
				{
					uint16& SRDOffset = RunningSRDOffsetPerFrame[BufferIndex];
					uint32& LineNumber = RunningLineNumberPerFrame[BufferIndex];
					const uint32 HeaderIndex = PacketIndex + (FrameInBlockIndex * PacketCount);
					if (ensure(Stream.StreamMemory.RTPHeaders[MemblockIndex].IsValidIndex(HeaderIndex)))
					{
						FVideoRTPHeader& CurrentHeader = Stream.StreamMemory.RTPHeaders[MemblockIndex][HeaderIndex];

						UpdateVideoRtpHeader(
							CurrentHeader,
							PacketIndex,
							SRDOffset,
							LineNumber,
							0, //SequenceNumber
							0, //Timestamp
							Stream.Options.GetStreamOptions<FRivermaxVideoOutputOptions>(Stream.StreamType)->AlignedResolution,
							Stream.FormatInfo,
							Stream.StreamMemory
						);
					}

					++BufferIndex;
				}
			}
		}

	private:
		TArray<uint16> RunningSRDOffsetPerFrame;
		TArray<uint32> RunningLineNumberPerFrame;
		FRivermaxOutVideoStream& Stream;
	};

	bool FRivermaxOutVideoStream::PushFrame(TSharedPtr<IRivermaxOutputInfo> FrameInfo)
	{
		if (!bIsActive)
		{
			return false;
		}

		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutVideoStream::PushVideoFrame);
		TSharedPtr<FRivermaxOutputVideoFrameInfo> NewFrameInfo = StaticCastSharedPtr<FRivermaxOutputVideoFrameInfo>(FrameInfo);
		NewFrameInfo->Stride = GetRowSizeInBytes();
		NewFrameInfo->Height = Options.GetStreamOptions<FRivermaxVideoOutputOptions>(StreamType)->AlignedResolution.Y;

		// Clear reserved frame if there is one. If not, try to get free frame.
		// ReservedFrame should always be valid when block on reservation mode is used.
		TSharedPtr<FRivermaxOutputFrame> ReservedFrame;
		if (!ReservedFrames.RemoveAndCopyValue(FrameInfo->FrameIdentifier, ReservedFrame))
		{
			check(Options.FrameLockingMode != EFrameLockingMode::BlockOnReservation);
			ReservedFrame = FrameManager->GetFreeFrame();
		}

		// If this is invalid it means that frame locking mode is BlockOnReservation and the render ran faster than media output fps.
		if (ReservedFrame.IsValid())
		{
			return FrameManager->SetFrameData(NewFrameInfo, ReservedFrame);
		}
		
		return false;
	}

	bool FRivermaxOutVideoStream::InitializeStreamMemoryConfig()
	{
		using namespace UE::RivermaxCore::Private::Utils;
		TSharedPtr<FRivermaxVideoOutputOptions> StreamOptions = Options.GetStreamOptions<FRivermaxVideoOutputOptions>(StreamType);
		FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(StreamOptions->PixelFormat);

		// Verify resolution for sampling type
		if (StreamOptions->AlignedResolution.X % FormatInfo.PixelGroupCoverage != 0)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't create Rivermax Output Stream. Aligned horizontal resolution of %d doesn't align with pixel group coverage of %d."), StreamOptions->AlignedResolution.X, FormatInfo.PixelGroupCoverage);
			return false;
		}

		const SIZE_T BytesPerRow = GetRowSizeInBytes();
		const SIZE_T FrameSize = BytesPerRow * StreamOptions->AlignedResolution.Y;

		if (FrameSize == 0)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't create Rivermax Output Stream. Frame size of 0 is invalid. Verify resolution."));
			return false;
		}
		


		// By default we want to divide the bytes evenly across packets. Some resolutions will require packets to be sized unevenly.
		const bool bFoundPayload = FindPayloadSize(BytesPerRow, FormatInfo.PixelGroupSize, StreamMemory.PayloadSize);
		if (bFoundPayload == false)
		{
			// Find out payload we want to use. Either we go the 'potential' multi SRD route or we restrict the stream based on supported resolutions.
			if (CVarRivermaxOutputEnableMultiSRD.GetValueOnAnyThread() >= 1)
			{
				UE_LOG(LogRivermax, Log, TEXT("Due to resolution %dx%d, row data will be sent over multiple packets with varied sizes."), StreamOptions->AlignedResolution.X, StreamOptions->AlignedResolution.Y);
				if (CVarRivermaxOutputMaximizePacketSize.GetValueOnAnyThread() >= 1)
				{
					StreamMemory.PayloadSize = GetMaximizedPayloadSize(FormatInfo.Sampling);
				}
				else
				{
					StreamMemory.PayloadSize = GetPayloadSize(FormatInfo.Sampling);
				}
			}
			else
			{
				UE_LOG(LogRivermax, Warning, TEXT("Could not find payload size for desired resolution %dx%d for desired pixel format."
					"If the intention is to use non standard resolutions, users might want to enable multi-srd support via Rivermax.Output.EnableMultiSRD."), StreamOptions->AlignedResolution.X, StreamOptions->AlignedResolution.Y);
				return false;
			}
		}

		// With payload size in hand, figure out how many packets we will need, how many chunks (group of packets) and configure descriptor arrays
		const uint32 PixelCount = StreamOptions->AlignedResolution.X * StreamOptions->AlignedResolution.Y;
		const uint64 FrameSizeInBytes = PixelCount / FormatInfo.PixelGroupCoverage * FormatInfo.PixelGroupSize;

		StreamMemory.PixelGroupPerPacket = StreamMemory.PayloadSize / FormatInfo.PixelGroupSize;
		StreamMemory.PixelsPerPacket = StreamMemory.PixelGroupPerPacket * FormatInfo.PixelGroupCoverage;

		// We might need a smaller packet to complete the end of frame so ceil to the next value
		StreamMemory.PacketsPerFrame = FMath::CeilToInt32((float)PixelCount / StreamMemory.PixelsPerPacket);

		// Depending on resolution and payload size, last packet of a line might not be fully utilized but we need the remaining bytes so ceil to next value
		StreamMemory.PacketsInLine = FMath::CeilToInt32((float)StreamMemory.PacketsPerFrame / StreamOptions->AlignedResolution.Y);

		StreamMemory.LinesInChunk = FindLinesPerChunk(Options);
		StreamMemory.PacketsPerChunk = StreamMemory.LinesInChunk * StreamMemory.PacketsInLine;
		StreamMemory.FramesFieldPerMemoryBlock = CachedCVars.bUseSingleMemblock ? Options.NumberOfBuffers : 1;

		// Chunk count won't necessarily align with the number of packets required. We need an integer amount of chunks to initialize our stream
		// and calculate how many packets that represents. Rivermax will expect the payload/header array to be that size. It just means that
		// we will mark the extra packets as 0 size.
		StreamMemory.ChunksPerFrameField = FMath::CeilToInt32((float)StreamMemory.PacketsPerFrame / StreamMemory.PacketsPerChunk);
		const uint64 RealPacketsPerFrame = StreamMemory.ChunksPerFrameField * StreamMemory.PacketsPerChunk;
		StreamMemory.PacketsPerMemoryBlock = RealPacketsPerFrame * StreamMemory.FramesFieldPerMemoryBlock;
		StreamMemory.ChunksPerMemoryBlock = StreamMemory.FramesFieldPerMemoryBlock * StreamMemory.ChunksPerFrameField;
		StreamMemory.MemoryBlockCount = Options.NumberOfBuffers / StreamMemory.FramesFieldPerMemoryBlock;

		// Setup arrays with the right sizes so we can give pointers to rivermax. This makes the stream header sizes static.
		StreamMemory.RTPHeaders.SetNumZeroed(StreamMemory.MemoryBlockCount);
		StreamMemory.PayloadSizes.SetNumZeroed(StreamMemory.PacketsPerMemoryBlock);
		StreamMemory.HeaderSizes.SetNumZeroed(StreamMemory.PacketsPerMemoryBlock);

		// Packed size should be 26 octets.
		StreamMemory.HeaderStrideSize = sizeof(FVideoRTPHeader);

		if (!SetupFrameManagement())
		{
			return false;
		}

		FRTPHeaderPrefiller RTPFiller(*this);

		// Used to keep track of acquired frames to prevent the same frame fomr being acquired from the pool more than once. 
		// When this scope is exited all frames are returned back to the Frame Manager pool.
		TArray<TSharedPtr<UE::RivermaxCore::Private::FRivermaxOutputFrame>> ProcessedFrames;

		StreamMemory.MemoryBlocks.SetNumZeroed(StreamMemory.MemoryBlockCount);
		CachedAPI->rmx_output_media_init_mem_blocks(StreamMemory.MemoryBlocks.GetData(), StreamMemory.MemoryBlockCount);
		for (uint32 BlockIndex = 0; BlockIndex < StreamMemory.MemoryBlockCount; ++BlockIndex)
		{
			rmx_output_media_mem_block& Block = StreamMemory.MemoryBlocks[BlockIndex];
			CachedAPI->rmx_output_media_set_chunk_count(&Block, StreamMemory.ChunksPerMemoryBlock);

			// We have two sub block, header and data
			constexpr uint8 SubBlockCount = 2;
			CachedAPI->rmx_output_media_set_sub_block_count(&Block, SubBlockCount);

			// Describe Header block
			CachedAPI->rmx_output_media_set_packet_layout(&Block, StreamMemory.HeaderBlockID, StreamMemory.HeaderSizes.GetData());

			// Describe Data block
			CachedAPI->rmx_output_media_set_packet_layout(&Block, StreamMemory.DataBlockID, StreamMemory.PayloadSizes.GetData());

			rmx_mem_multi_key_region* DataMemory;
			DataMemory = CachedAPI->rmx_output_media_get_dup_sub_block(&Block, StreamMemory.DataBlockID);
			if (DataMemory == nullptr)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Faild to get payload memory block. Output stream won't be created."));
				return false;
			}

			// If intermediate buffer is used, we setup rmax memblock to use that address. Otherwise, we map it to our actual frame's address
			if (StreamMemory.bUseIntermediateBuffer)
			{
				DataMemory->addr = Allocator->GetFrameAddress(BlockIndex);
			}
			else
			{
				TSharedPtr<UE::RivermaxCore::Private::FRivermaxOutputFrame> ProcessedFrame = FrameManager->GetFreeFrame();
				DataMemory->addr = ProcessedFrame->Buffer;
				ProcessedFrames.Add(ProcessedFrame);
			}

			DataMemory->length = StreamMemory.ChunksPerFrameField * StreamMemory.PacketsPerChunk * StreamMemory.PayloadSize;

			constexpr rmx_mkey_id InvalidKey = ((rmx_mkey_id)(-1L));
			DataMemory->mkey[0] = InvalidKey;
			DataMemory->mkey[1] = InvalidKey;


			rmx_mem_multi_key_region* HeaderMemory;
			HeaderMemory = CachedAPI->rmx_output_media_get_dup_sub_block(&Block, StreamMemory.HeaderBlockID);
			if (HeaderMemory == nullptr)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Faild to get header memory block. Output stream won't be created."));
				return false;
			}

			StreamMemory.RTPHeaders[BlockIndex].SetNumZeroed(RealPacketsPerFrame * StreamMemory.FramesFieldPerMemoryBlock);
			HeaderMemory->addr = &StreamMemory.RTPHeaders[BlockIndex][0];
			HeaderMemory->length = StreamMemory.HeaderStrideSize;
			HeaderMemory->mkey[0] = InvalidKey;
			HeaderMemory->mkey[1] = InvalidKey;
		}
		uint64 ProcessedBytes = 0;
		uint64 LineSize = 0;
		for (int32 PayloadSizeIndex = 0; PayloadSizeIndex < RealPacketsPerFrame; ++PayloadSizeIndex)
		{
			uint32 HeaderSize = FVideoRTPHeader::OneSRDSize;
			uint32 ThisPayloadSize = StreamMemory.PayloadSize;
			if (ProcessedBytes < FrameSizeInBytes)
			{
				if ((LineSize + StreamMemory.PayloadSize) == BytesPerRow)
				{
					LineSize = 0;
				}
				else if ((LineSize + StreamMemory.PayloadSize) > BytesPerRow)
				{
					HeaderSize = FVideoRTPHeader::TwoSRDSize;
					LineSize = StreamMemory.PayloadSize - (BytesPerRow - LineSize);
					if (LineSize > BytesPerRow)
					{
						UE_LOG(LogRivermax, Warning, TEXT("Unsupported small resolution, %dx%d, needing more than 2 SRD to express"), StreamOptions->AlignedResolution.X, StreamOptions->AlignedResolution.Y);
						return false;
					}
				}
				else
				{
					// Keep track of line size offset to know when to use TwoSRDs
					LineSize += StreamMemory.PayloadSize;
				}

				// This means that this is the header for the last packet and it is smaller than all the previous ones.
				if ((ProcessedBytes + StreamMemory.PayloadSize) > FrameSizeInBytes)
				{
					HeaderSize = FVideoRTPHeader::OneSRDSize;
					ThisPayloadSize = FrameSizeInBytes - ProcessedBytes;
				}
			}
			else
			{
				// Extra header/payload required for the chunk alignment are set to 0. Nothing has to be sent out the wire.
				HeaderSize = 0;
				ThisPayloadSize = 0;
			}

			// All buffers are configured the same so compute header and payload sizes once and assigned to all impacted locations
			for (uint32 BufferIndex = 0; BufferIndex < StreamMemory.FramesFieldPerMemoryBlock; ++BufferIndex)
			{
				StreamMemory.HeaderSizes[PayloadSizeIndex + (BufferIndex * RealPacketsPerFrame)] = HeaderSize;
				StreamMemory.PayloadSizes[PayloadSizeIndex + (BufferIndex * RealPacketsPerFrame)] = ThisPayloadSize;
			}

			if (CachedCVars.bPrefillRTPHeaders && HeaderSize > 0)
			{
				RTPFiller.Update(PayloadSizeIndex);
			}

			ProcessedBytes += ThisPayloadSize;
		}

		// Verify memcopy config to make sure it works for current frame size / chunking
		if (StreamMemory.bUseIntermediateBuffer)
		{
			StreamMemory.FrameMemorySliceCount = FMath::Clamp(CVarRivermaxOutputMaxFrameMemorySliceCount.GetValueOnAnyThread(), 1, 100);
			StreamMemory.ChunkSpacingBetweenMemcopies = FMath::Clamp(CVarRivermaxOutputMemcopyChunkSpacing.GetValueOnAnyThread(), 1, 20);

			const uint32 ChunkRequired = StreamMemory.ChunkSpacingBetweenMemcopies * StreamMemory.FrameMemorySliceCount;
			if (ChunkRequired > 0 && ChunkRequired > StreamMemory.ChunksPerFrameField)
			{
				// Favor reducing number of memcopies. If required packet is smaller, chances are it's a small frame size
				// so memcopies will be smaller.
				const double Ratio = StreamMemory.ChunksPerFrameField / (double)ChunkRequired;
				StreamMemory.FrameMemorySliceCount *= Ratio;
			}
		}

		return true;
	}

	bool FRivermaxOutVideoStream::IsFrameAvailableToSend()
	{
		return FrameManager->IsFrameAvailableToSend();
	}

	bool FRivermaxOutVideoStream::CopyFrameData(const TSharedPtr<FRivermaxOutputFrame>& SourceFrame, uint8* DestinationBase)
	{
		// Make sure copy size doesn't go over frame size
		const SIZE_T FrameSize = GetRowSizeInBytes() * Options.GetStreamOptions<FRivermaxVideoOutputOptions>(StreamType)->AlignedResolution.Y;
		if (ensure(FrameSize > 0))
		{
			const SIZE_T BlockSize = 1 + ((FrameSize - 1) / StreamMemory.FrameMemorySliceCount);
			const SIZE_T MaxSize = FrameSize - SourceFrame->Offset;
			const SIZE_T CopySize = FMath::Min(BlockSize, MaxSize);

			// Copy data until we have covered the whole frame. Last block might be smaller.
			if (CopySize > 0)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CopyFrameData);
				const SIZE_T PacketsToCopy = CopySize / StreamMemory.PayloadSize;
				const SIZE_T PacketOffset = SourceFrame->ChunkNumber * StreamMemory.PacketsPerChunk;
				const uint64 PacketDeltaTime = (StreamData.FrameFieldTimeIntervalNs - TransmitOffsetNanosec) / StreamMemory.PacketsPerFrame;

				FCopyArgs Args;
				uint8* SourceStart = reinterpret_cast<uint8*>(SourceFrame->Buffer);
				uint8* DestinationStart = DestinationBase;
				Args.SourceMemory = SourceStart + SourceFrame->Offset;
				Args.DestinationMemory = DestinationStart + SourceFrame->Offset;
				Args.SizeToCopy = CopySize;

				// Update memory offset for next copy
				SourceFrame->Offset += CopySize;

				Allocator->CopyData(Args);

				return true;
			}
		}
		else
		{
			UE_LOG(LogRivermax, Error, TEXT("Invalid frame size detected while stream was active. Shutting down."));
			Listener->OnStreamError();
			Stop();
		}

		return false;
	}

	bool FRivermaxOutVideoStream::SetupFrameManagement()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutVideoStream::SetupFrameManagement);

		FrameManager = MakeUnique<FFrameManager>();
		TSharedPtr<FRivermaxVideoOutputOptions> VideoOptions = Options.GetStreamOptions<FRivermaxVideoOutputOptions>(StreamType);

		// We do (try) to make gpu allocations here to let the capturer know if we require it or not.
		bool bTryGPUDirect = RivermaxModule->GetRivermaxManager()->IsGPUDirectOutputSupported() && VideoOptions->bUseGPUDirect;
		if (bTryGPUDirect)
		{
			const ERHIInterfaceType RHIType = RHIGetInterfaceType();
			if (RHIType != ERHIInterfaceType::D3D12)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. RHI is %d but only Dx12 is supported at the moment."), RHIType);
				bTryGPUDirect = false;
			}
		}

		// Work around when dealing with multi memblocks. Rivermax fails to create stream 
		// With memblock not starting on the right cuda alignment. 
		const bool bAlignEachFrameMemory = !CachedCVars.bUseSingleMemblock;
		FFrameManagerSetupArgs FrameManagerArgs;
		FrameManagerArgs.Resolution = VideoOptions->AlignedResolution;
		FrameManagerArgs.bTryGPUAllocation = bTryGPUDirect;
		FrameManagerArgs.NumberOfFrames = Options.NumberOfBuffers;
		FrameManagerArgs.Stride = GetRowSizeInBytes();
		FrameManagerArgs.FrameDesiredSize = StreamMemory.ChunksPerFrameField * StreamMemory.PacketsPerChunk * StreamMemory.PayloadSize;
		FrameManagerArgs.bAlignEachFrameAlloc = bAlignEachFrameMemory;
		FrameManagerArgs.OnFreeFrameDelegate = FOnFrameReadyDelegate::CreateRaw(this, &FRivermaxOutVideoStream::OnFrameReadyToBeUsed);
		FrameManagerArgs.OnPreFrameReadyDelegate = FOnFrameReadyDelegate::CreateRaw(this, &FRivermaxOutVideoStream::OnPreFrameReadyToBeSent);
		FrameManagerArgs.OnFrameReadyDelegate = FOnFrameReadyDelegate::CreateRaw(this, &FRivermaxOutVideoStream::OnFrameReadyToBeSent);
		FrameManagerArgs.OnCriticalErrorDelegate = FOnCriticalErrorDelegate::CreateRaw(this, &FRivermaxOutVideoStream::OnFrameManagerCriticalError);
		const EFrameMemoryLocation FrameLocation = FrameManager->Initialize(FrameManagerArgs);
		bUseGPUDirect = FrameLocation == EFrameMemoryLocation::GPU;


		// Only support intermediate buffer for alignment point method to avoid running into chunk issue when repeating a frame
		const bool bHasAllocatedFrames = (FrameLocation != EFrameMemoryLocation::None);
		if (bHasAllocatedFrames && Options.AlignmentMode == ERivermaxAlignmentMode::AlignmentPoint && CVarRivermaxOutputEnableIntermediateBuffer.GetValueOnAnyThread())
		{
			// Allocate intermediate buffer in same memory space as frame memory.
			FOnFrameDataCopiedDelegate OnDataCopiedDelegate = FOnFrameDataCopiedDelegate::CreateRaw(this, &FRivermaxOutVideoStream::OnMemoryChunksCopied);

			const int32 DesiredSize = FrameManagerArgs.FrameDesiredSize * StreamMemory.FramesFieldPerMemoryBlock;
			if (bUseGPUDirect)
			{
				Allocator = MakeUnique<FGPUAllocator>(DesiredSize, OnDataCopiedDelegate);
			}
			else
			{
				Allocator = MakeUnique<FSystemAllocator>(DesiredSize, OnDataCopiedDelegate);
			}

			if (!Allocator->Allocate(StreamMemory.MemoryBlockCount, bAlignEachFrameMemory))
			{
				return false;
			}

			StreamMemory.bUseIntermediateBuffer = true;
		}

		// Cache buffer addresses used by rivermax in order to start copying into it early on
		StreamMemory.BufferAddresses.Reserve(Options.NumberOfBuffers);
		if (StreamMemory.bUseIntermediateBuffer)
		{
			check(Allocator);

			for (uint32 MemblockIndex = 0; MemblockIndex < StreamMemory.MemoryBlockCount; ++MemblockIndex)
			{
				uint8* BaseAddress = static_cast<uint8*>(Allocator->GetFrameAddress(MemblockIndex));
				for (uint32 FrameIndex = 0; FrameIndex < StreamMemory.FramesFieldPerMemoryBlock; ++FrameIndex)
				{
					uint8* FrameAddress = BaseAddress + (FrameIndex * FrameManagerArgs.FrameDesiredSize);
					StreamMemory.BufferAddresses.Add(FrameAddress);
				}
			}
		}
		else
		{
			// Used to keep track of acquired frames to prevent the same frame from being acquired from the pool more than once. 
			// When this scope is exited all frames are returned back to the Frame Manager pool.
			TArray<TSharedPtr<UE::RivermaxCore::Private::FRivermaxOutputFrame>> ProcessedFrames;

			// When we don't use intermediate buffer, each frame has its own buffer addresses and we don't need to look at memblocks
			for (int32 BufferIndex = 0; BufferIndex < Options.NumberOfBuffers; ++BufferIndex)
			{
				TSharedPtr<UE::RivermaxCore::Private::FRivermaxOutputFrame> Frame = FrameManager->GetFreeFrame();
				StreamMemory.BufferAddresses.Add(Frame->Buffer);
				ProcessedFrames.Add(Frame);
			}
		}
		check(StreamMemory.BufferAddresses.Num() == Options.NumberOfBuffers);

		return FrameLocation != EFrameMemoryLocation::None;
	}

	void FRivermaxOutVideoStream::CleanupFrameManagement()
	{
		if (FrameManager)
		{
			FrameManager->Cleanup();
			FrameManager.Reset();
		}

		if (Allocator)
		{
			Allocator->Deallocate();
			Allocator.Reset();
		}
	}

	SIZE_T FRivermaxOutVideoStream::GetRowSizeInBytes() const
	{
		check(FormatInfo.PixelGroupCoverage != 0);
		TSharedPtr<FRivermaxVideoOutputOptions> VideoOptions = StaticCastSharedPtr<FRivermaxVideoOutputOptions>(Options.StreamOptions[(uint8)StreamType]);
		return (VideoOptions->AlignedResolution.X / FormatInfo.PixelGroupCoverage) * FormatInfo.PixelGroupSize;
	}

	void FRivermaxOutVideoStream::SetupRTPHeaders()
	{
		FVideoRTPHeader* FirstHeaderPtr = reinterpret_cast<FVideoRTPHeader*>(CurrentFrame->HeaderPtr);
		check(FirstHeaderPtr);

		for (uint32 PacketIndex = 0; PacketIndex < StreamMemory.PacketsPerChunk && CurrentFrame->PacketCounter < StreamMemory.PacketsPerFrame; ++PacketIndex)
		{
			using namespace UE::RivermaxCore::Private::Utils;
			FVideoRTPHeader* HeaderRawPtr = FirstHeaderPtr+ PacketIndex;
			// Since the static part of the RTP header is filled on initalization we only need to update the 
			// non static parts.
			if (CachedCVars.bPrefillRTPHeaders)
			{
				HeaderRawPtr->RTPHeader.SequenceNumber = ByteSwap((uint16)(StreamData.SequenceNumber & 0xFFFF));
				HeaderRawPtr->RTPHeader.Timestamp = ByteSwap(CurrentFrame->MediaTimestamp);
				HeaderRawPtr->RTPHeader.ExtendedSequenceNumber = ByteSwap((uint16)((StreamData.SequenceNumber >> 16) & 0xFFFF));
			}
			else
			{
				TSharedPtr<FRivermaxVideoOutputOptions> VideoOptions = StaticCastSharedPtr<FRivermaxVideoOutputOptions>(Options.StreamOptions[(uint8)StreamType]);
				UpdateVideoRtpHeader(
					*HeaderRawPtr,
					CurrentFrame->PacketCounter,
					CurrentFrame->SRDOffset,
					CurrentFrame->LineNumber,
					StreamData.SequenceNumber,
					CurrentFrame->MediaTimestamp,
					VideoOptions->AlignedResolution,
					FormatInfo,
					StreamMemory
				);
			}

			++StreamData.SequenceNumber;
			++CurrentFrame->PacketCounter;
		}
	}

	void FRivermaxOutVideoStream::LogStreamDescriptionOnCreation() const
	{
		FRivermaxOutStream::LogStreamDescriptionOnCreation();

		TSharedPtr<FRivermaxVideoOutputOptions> VideoOptions = StaticCastSharedPtr<FRivermaxVideoOutputOptions>(Options.StreamOptions[(uint8)StreamType]);

		TStringBuilder<512> StreamDescription;
		if (bUseGPUDirect)
		{
			StreamDescription.Appendf(TEXT("Using GPUDirect."));
		}

		StreamDescription.Appendf(TEXT("Settings: Resolution = %dx%d, "), VideoOptions->AlignedResolution.X, VideoOptions->AlignedResolution.Y);
		StreamDescription.Appendf(TEXT("FrameRate = %s, "), *VideoOptions->FrameRate.ToPrettyText().ToString());
		StreamDescription.Appendf(TEXT("Pixel format = %s, "), LexToString(VideoOptions->PixelFormat));
		StreamDescription.Appendf(TEXT("Alignment = %s, "), LexToString(Options.AlignmentMode));
		StreamDescription.Appendf(TEXT("Framelocking = %s."), LexToString(Options.FrameLockingMode));

		UE_LOG(LogRivermax, Display, TEXT("%s"), *FString(StreamDescription));
	}

	bool FRivermaxOutVideoStream::ReserveFrame(uint64 FrameCounter) const
	{
		if (!bIsActive)
		{
			return false;
		}

		// There is only one reserved frame at the time per stream.
		TSharedPtr<FRivermaxOutputFrame> ReservedFrame = FrameManager->GetFreeFrame();
		if (!ReservedFrame.IsValid() && Options.FrameLockingMode == EFrameLockingMode::BlockOnReservation)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Rmax::WaitForAvailableFrame);
			while (!ReservedFrame && bIsActive)
			{
				FrameAvailableSignal->Wait();
				ReservedFrame = FrameManager->GetFreeFrame();
			}
		}

		if (ReservedFrame.IsValid() && bIsActive)
		{
			ReservedFrame->SetFrameCounter(FrameCounter);
			ReservedFrames.Add(FrameCounter, ReservedFrame);
		}

		return ReservedFrame.IsValid();
	}

	TSharedPtr<FRivermaxOutputFrame> FRivermaxOutVideoStream::GetNextFrameToSend(bool bWait)
	{
		TSharedPtr<FRivermaxOutputFrame> NextFrameToSend = FrameManager->DequeueFrameToSend();

		if (bWait)
		{
			while (!NextFrameToSend.IsValid() && bIsActive)
			{
				FrameReadyToSendSignal->Wait();
				NextFrameToSend = FrameManager->DequeueFrameToSend();
			}
		}
		return NextFrameToSend;
	}

	void FRivermaxOutVideoStream::CompleteCurrentFrame(bool bReleaseFrame)
	{
		FRivermaxOutStream::CompleteCurrentFrame(bReleaseFrame);

		// We don't release when there is no new frame, so we keep a hold on it to repeat it.
		if (bReleaseFrame)
		{
			FrameManager->FrameSentEvent();
		}
	}
}

