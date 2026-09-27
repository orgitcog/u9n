// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

#define UE_API WEBMMEDIA_API

class IWebMSamplesSink;
class FWebMMediaAudioSample;
class FWebMMediaAudioSamplePool;
struct FWebMFrame;
struct OpusDecoder;

class FWebMAudioDecoder
{
public:
	UE_API FWebMAudioDecoder(IWebMSamplesSink& InSamples);
	UE_API ~FWebMAudioDecoder();

public:
	UE_API bool Initialize(const char* InCodec, int32 InSampleRate, int32 InChannels, const uint8* CodecPrivateData, size_t CodecPrivateDataSize);
	UE_API void DecodeAudioFramesAsync(const TArray<TSharedPtr<FWebMFrame>>& AudioFrames);
	UE_API bool IsBusy() const;

private:
	enum ESupportedCodecs
	{
		Opus,
		Vorbis,
	};

	struct FVorbisDecoder;
	TUniquePtr<FWebMMediaAudioSamplePool> AudioSamplePool;
	TUniquePtr<FVorbisDecoder> VorbisDecoder;
	FGraphEventRef AudioDecodingTask;
	TArray<uint8> DecodeBuffer;
	IWebMSamplesSink& Samples;
	OpusDecoder* OpusDecoder;
	ESupportedCodecs Codec;
	int32 FrameSize;
	int32 SampleRate;
	int32 Channels;

	void InitializeOpus();
	bool InitializeVorbis(const uint8* CodecPrivateData, size_t CodecPrivateDataSize);
	void DoDecodeAudioFrames(const TArray<TSharedPtr<FWebMFrame>>& AudioFrames);
	int32 DecodeOpus(const TSharedPtr<FWebMFrame>& AudioFrame);
	int32 DecodeVorbis(const TSharedPtr<FWebMFrame>& AudioFrame);
};

#undef UE_API
