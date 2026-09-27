// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraAudioSample.h"

FElectraAudioSample::~FElectraAudioSample()
{
	FMemory::Free(Buffer);
}

bool FElectraAudioSample::Initialize(const void* InData, EMediaAudioSampleFormat InFormat, uint32 InNumChannels, uint32 InNumFrames, uint32 InSampleRate, const FMediaTimeStamp& InTime, const FTimespan& InDuration)
{
	check(InFormat == EMediaAudioSampleFormat::Float);

	uint32 NumBytesNeeded = sizeof(float) * InNumChannels * InNumFrames;
	if (NumBytesNeeded > NumBytesAllocated)
	{
		Buffer = FMemory::Realloc(Buffer, NumBytesNeeded);
		if (!Buffer)
		{
			NumBytesAllocated = 0;
			return false;
		}
		NumBytesAllocated = NumBytesNeeded;
	}
	MediaAudioSampleFormat = InFormat;
	NumChannels = InNumChannels;
	NumFrames = InNumFrames;
	SampleRate = InSampleRate;
	MediaTimeStamp = InTime;
	Duration = InDuration;
	if (Buffer && InData)
	{
		FMemory::Memcpy(Buffer, InData, NumBytesNeeded);
	}
	return true;
}

#if !UE_SERVER
void FElectraAudioSample::ShutdownPoolable()
{
	// Nothing to do here at the moment.
}
#endif
