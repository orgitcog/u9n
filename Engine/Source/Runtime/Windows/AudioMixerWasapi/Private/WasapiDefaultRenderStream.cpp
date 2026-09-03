// Copyright Epic Games, Inc. All Rights Reserved.

#include "WasapiDefaultRenderStream.h"

#include "AudioMixerWasapiLog.h"
#include "WasapiAudioUtils.h"

namespace Audio
{
	bool FWasapiDefaultRenderStream::InitializeHardware(const FWasapiRenderStreamParams& InParams)
	{
		if (FAudioMixerWasapiRenderStream::InitializeHardware(InParams))
		{
			NumPartialBuffersToWrite = 1;
			const uint32 MinBufferSize = GetMinimumBufferSize(InParams.SampleRate);			
			if (MinBufferSize > InParams.NumFrames)
			{
				NumPartialBuffersToWrite = FMath::DivideAndRoundUp(MinBufferSize, InParams.NumFrames);
			}

			WriteNumFrames = NumPartialBuffersToWrite * InParams.NumFrames;
			AudioBufferTotalBytes = WriteNumFrames * AudioFormat.GetFrameSizeInBytes();
			PartialBufferNumBytes = InParams.NumFrames * AudioFormat.GetFrameSizeInBytes();

			return true;
		}
		
		return false;
	}
	
	bool FWasapiDefaultRenderStream::TeardownHardware()
	{
		ReadNextBufferDelegate.Unbind();

		return FAudioMixerWasapiRenderStream::TeardownHardware();
	}

	void FWasapiDefaultRenderStream::DeviceRenderCallback()
	{
		SCOPED_NAMED_EVENT(FWasapiDefaultRenderStream_DeviceRenderCallback, FColor::Blue);

		if (bIsInitialized)
		{
			uint32 NumFramesPadding = 0;
			AudioClient->GetCurrentPadding(&NumFramesPadding);

			// NumFramesPerDeviceBuffer is the buffer size WASAPI allocated. It is guaranteed to 
			// be at least the amount requested. For example, if we request a 1024 frame buffer, WASAPI
			// might allocate a 1056 frame buffer. The padding is subtracted from the allocated amount
			// to determine how much space is available currently in the buffer.
			const int32 NumFramesAvailable = NumFramesPerDeviceBuffer - NumFramesPadding;
			
			if (NumFramesAvailable >= WriteNumFrames)
			{
				check(RenderBufferView.IsEmpty());

				uint8* BufferStartPtr = nullptr;
				if (SUCCEEDED(RenderClient->GetBuffer(WriteNumFrames, &BufferStartPtr)))
				{
					TArrayView<uint8> BufferStartView(BufferStartPtr, AudioBufferTotalBytes);
					
					for (int32 i = 0; i < NumPartialBuffersToWrite; ++i)
					{
						const SIZE_T ByteOffset = i * PartialBufferNumBytes;
						RenderBufferView = BufferStartView.Slice(ByteOffset, AudioBufferTotalBytes - ByteOffset);
						
						if (!ReadNextBufferDelegate.ExecuteIfBound())
						{
							++CallbackBufferErrors;
						}
					}

					HRESULT Result = RenderClient->ReleaseBuffer(WriteNumFrames, 0 /* flags */);
					if (FAILED(Result))
					{
						++CallbackBufferErrors;
					}

					RenderBufferView = TArrayView<uint8>();
				}
				else
				{
					++CallbackBufferErrors;
				}
			}
		}
	}

	void FWasapiDefaultRenderStream::SubmitBuffer(const uint8* InBuffer, const SIZE_T InNumFrames)
	{
		if (RenderBufferView.Num() >= InNumFrames)
		{
			check(InNumFrames == RenderStreamParams.NumFrames);
			const SIZE_T NumBytes = InNumFrames * AudioFormat.GetFrameSizeInBytes();

			FMemory::Memcpy(RenderBufferView.GetData(), InBuffer, NumBytes);
		}
	}
}
