// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixDsp/Streaming/TrackChannelInfo.h"
#include "HarmonixDsp/AudioDataRenderer.h"

#include "Templates/SharedPointer.h"

#include "DSP/BufferVectorOperations.h"
#include "DSP/ConvertDeinterleave.h"
#include "DSP/MultichannelBuffer.h"
#include "DSP/MultichannelLinearResampler.h"

#define UE_API HARMONIXDSP_API

class FSoundWaveProxy;
class FSoundWaveProxyReader;
class FFusionSampler;
class IStretcherAndPitchShifter;

DECLARE_LOG_CATEGORY_EXTERN(LogHarmonixStreamingAudioRendererV2, Log, All);

class FStreamingAudioRendererV2 : public IAudioDataRenderer
{
public:

	using FAlignedInt16Buffer = TArray<int16, Audio::FAudioBufferAlignedAllocator>;

	UE_API FStreamingAudioRendererV2();
	UE_API ~FStreamingAudioRendererV2();

	UE_API virtual void Reset() override;
	UE_API virtual void SetAudioData(TSharedRef<FSoundWaveProxy> SoundWaveProxy, const FSettings& InSettings) override;
	UE_API virtual const TSharedPtr<FSoundWaveProxy> GetAudioData() const override;
	
	UE_API virtual void MigrateToSampler(const FFusionSampler* InSampler) override;

	UE_API virtual void SetFrame(uint32 InFrameNum) override;

	UE_API virtual double Render(TAudioBuffer<float>& OutBuffer, double InPos, int32 InMaxFrame, double InResampleInc, double InPitchShift, double InSpeed,
		bool MaintainPitchWhenSpeedChanges, bool InShouldHonorLoopPoints, const FGainMatrix& InGain) override;

	UE_API double RenderInternal(TAudioBuffer<float>& OutBuffer, double Pos, int32 MaxFrame, double Inc, bool ShouldHonorLoopPoints, const FGainMatrix& Gain);

	UE_API virtual double RenderUnshifted(TAudioBuffer<float>& OutBuffer, double InPos, int32 InMaxFrame, double InInc,
		bool InShouldHonorLoopPoints, const FGainMatrix& InGain) override;

	UE_API void GenerateSourceAudio(uint32 StartFrame, Audio::FAlignedFloatBuffer& OutAudio, bool bHonorLoopRegion = false);

private:

	void RenderSimpleUnshifted(TAudioBuffer<float>& OutBuffer, const FLerpData* LerpArray, uint32 InNumFrames, const FGainMatrix& InGain, double InInc, bool ShouldHonorLoopPoints);
	void RenderMultiChannelUnshifted(TAudioBuffer<float>& OutBuffer, const FLerpData* LerpArray, uint32 InNumFrames, const FGainMatrix& InGain, double InInc, bool ShouldHonorLoopPoints);
	void RenderMultiChannelRoutedUnshifted(TAudioBuffer<float>& OutBuffer, const FLerpData* LerpArray, uint32 InNumFrames, const FGainMatrix& InGain, double InInc, bool ShouldHonorLoopPoints);

	void SeekSourceAudioToFrame(uint32 FrameIdx);
	void DecodeSourceAudio(Audio::TCircularAudioBuffer<float>& OutBuffer);

	void GenerateSourceAudioInternal(uint32 StartFrameIndex, float* OutAudioData, uint32 NumSamples);

	uint32 GetSourceAudioFrameIndex();

	static TUniquePtr<FSoundWaveProxyReader> CreateProxyReader(TSharedRef<FSoundWaveProxy> WaveProxy);

	bool HasLoopSection() const;
	uint32 GetLoopStartFrame() const;
	uint32 GetLoopEndFrame() const;

	const FFusionSampler* MySampler = nullptr;
	const TArray<FTrackChannelInfo>* TrackChannelInfo = nullptr;
	TSharedPtr<IStretcherAndPitchShifter, ESPMode::ThreadSafe> Shifter;

	// Ref to the actual streaming audio data
	// this data is a shared instance of audio data
	// it will get loaded on construction 
	TSharedPtr<FSoundWaveProxy> SoundWaveProxy;

	TUniquePtr<FSoundWaveProxyReader> WaveProxyReader;

	Audio::FAlignedFloatBuffer DecodeBuffer;
	Audio::FAlignedFloatBuffer WorkBuffer;

	Audio::TCircularAudioBuffer<float> InterleavedCircularBuffer;
	int32 NumDeinterleaveChannels;
	Audio::FAlignedFloatBuffer LastLoopFrameCache;
	bool bLastLoopFrameCached = false;


	// needs to be small enough to avoid audio artifacts and syncing issues
	// but also large enough that we're decoding multiple times per block
	static constexpr int32 DeinterleaveBlockSizeInFrames = 256;
	static constexpr uint32 MaxDecodeSizeInFrames = 1024;

	int32 CalculateNumFramesNeeded(const FLerpData* LerpData, int32 NumPoints);
};

#undef UE_API
