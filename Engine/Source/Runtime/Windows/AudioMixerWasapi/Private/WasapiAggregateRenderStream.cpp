// Copyright Epic Games, Inc. All Rights Reserved.

#include "WasapiAggregateRenderStream.h"

#include "AudioMixerWasapiLog.h"
#include "DSP/FloatArrayMath.h"
#include "WasapiAudioUtils.h"

namespace Audio
{
	bool FWasapiAggregateRenderStream::InitializeHardware(const FWasapiRenderStreamParams& InParams)
	{
		if (FAudioMixerWasapiRenderStream::InitializeHardware(InParams))
		{
			DirectOutBuffers.Reset();
			DirectOutBuffers.SetNum(InParams.HardwareDeviceInfo.NumChannels);
		
			for (TCircularAudioBuffer<float>& Buffer : DirectOutBuffers)
			{
				int32 NumOutputBuffers = FMath::Max(InParams.NumBuffers, 2);
				int32 BufferCapacity = InParams.NumFrames * NumOutputBuffers;

				Buffer.SetCapacity(BufferCapacity);
			}

			InterleaveBuffers.Reset();
			InterleaveBuffers.SetNum(InParams.HardwareDeviceInfo.NumChannels);

			const uint32 MinBufferSize = GetMinimumBufferSize(InParams.SampleRate);			
			WriteNumFrames = FMath::Max(MinBufferSize, InParams.NumFrames);

			for (FAlignedFloatBuffer& Buffer : InterleaveBuffers)
			{
				Buffer.SetNumZeroed(WriteNumFrames);
			}

			return true;
		}

		return false;
	}

	bool FWasapiAggregateRenderStream::StartAudioStream()
	{
		FAudioMixerWasapiRenderStream::StartAudioStream();

		return true;
	}

	void FWasapiAggregateRenderStream::SubmitDirectOutBuffer(const int32 InChannelIndex, const FAlignedFloatBuffer& InBuffer)
	{
		if (InChannelIndex >= 0 && InChannelIndex < RenderStreamParams.HardwareDeviceInfo.NumChannels)
		{
			TCircularAudioBuffer<float>& DirectOutBuffer = DirectOutBuffers[InChannelIndex];

			if (InBuffer.Num() == RenderStreamParams.NumFrames)
			{
				DirectOutBuffer.Push(InBuffer.GetData(), InBuffer.Num());
			}
		}
	}

	void FWasapiAggregateRenderStream::DeviceRenderCallback()
	{
		SCOPED_NAMED_EVENT(FWasapiAggregateRenderStream_DeviceRenderCallback, FColor::Blue);

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
				uint8* RenderBufferPtr = nullptr;

				if (SUCCEEDED(RenderClient->GetBuffer(WriteNumFrames, &RenderBufferPtr)))
				{
					InterleaveOutput((float*)RenderBufferPtr, WriteNumFrames);
					
					HRESULT Result = RenderClient->ReleaseBuffer(WriteNumFrames, 0 /* flags */);
					if (FAILED(Result))
					{
						++CallbackBufferErrors;
					}
				}
				else
				{
					++CallbackBufferErrors;
				}
			}
		}
	}

	void FWasapiAggregateRenderStream::InterleaveOutput(float* OutRenderBufferPtr, const uint32 InNumFrames)
	{
		const int32 NumChannels = InterleaveBuffers.Num();
		const int32 NumFrames = InterleaveBuffers[0].Num();

		if (NumFrames == InNumFrames)
		{
			for (int32 Index = 0; Index < DirectOutBuffers.Num(); ++Index)
			{
				// Clear out the interleave buffer
				InterleaveBuffers[Index].Reset();
				InterleaveBuffers[Index].SetNumZeroed(NumFrames);

				if (DirectOutBuffers[Index].Num() >= InNumFrames)
				{
					const int32 NumFramesPopped = DirectOutBuffers[Index].Pop(InterleaveBuffers[Index].GetData(), InNumFrames);
				}
			}

			TArray<const float*> BufferPtrArray;
			BufferPtrArray.Reset(NumChannels);

			for (const FAlignedFloatBuffer& Buffer : InterleaveBuffers)
			{
				const float* BufferPtr = Buffer.GetData();
				BufferPtrArray.Add(BufferPtr);
			}

			const float** InterleaveBufferPtr = BufferPtrArray.GetData();

			ArrayInterleave(InterleaveBufferPtr, OutRenderBufferPtr, NumFrames, NumChannels);
		}
	}
}
