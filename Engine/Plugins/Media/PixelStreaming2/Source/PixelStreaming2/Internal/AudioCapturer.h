// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DSP/MultithreadedPatching.h"
#include "AudioProducer.h"
#include "ISubmixBufferListener.h"
#include "SampleBuffer.h"
#include "TickableTask.h"

namespace UE::PixelStreaming2
{
	class FAudioCapturer;

	class PIXELSTREAMING2_API FAudioPatchMixer : public Audio::FPatchMixer
	{
	public:
		FAudioPatchMixer(uint8 NumChannels, uint32 SampleRate, float SampleSizeSeconds);
		virtual ~FAudioPatchMixer() = default;

		uint32 GetMaxBufferSize() const;
		uint8  GetNumChannels() const;
		uint32 GetSampleRate() const;

	protected:
		uint8  NumChannels;
		uint32 SampleRate;
		float  SampleSizeSeconds;
	};

	class PIXELSTREAMING2_API FPatchInputProxy : public IPixelStreaming2AudioProducer
	{
	public:
		FPatchInputProxy(TSharedPtr<FAudioPatchMixer> InMixer);
		virtual ~FPatchInputProxy() override;

		virtual void PushAudio(const float* AudioData, int32 InNumSamples, int32 InNumChannels, int32 InSampleRate) override;

	protected:
		TSharedPtr<FAudioPatchMixer> Mixer;
		Audio::FResampler			 Resampler;
		Audio::FPatchInput			 PatchInput;
		uint8						 NumChannels;
		uint32						 SampleRate;
	};

	class PIXELSTREAMING2_API FMixAudioTask : public FPixelStreamingTickableTask
	{
	public:
		FMixAudioTask(FAudioCapturer* Capturer, TSharedPtr<FAudioPatchMixer> Mixer);

		virtual ~FMixAudioTask() = default;

		// Begin FPixelStreamingTickableTask
		virtual void		   Tick(float DeltaMs) override;
		virtual const FString& GetName() const override;
		// End FPixelStreamingTickableTask

	protected:
		bool								  bIsRunning;
		Audio::VectorOps::FAlignedFloatBuffer MixingBuffer;

		FAudioCapturer*				 Capturer;
		TSharedPtr<FAudioPatchMixer> Mixer;
	};

	class PIXELSTREAMING2_API FAudioCapturer : public IPixelStreaming2AudioProducer
	{
	public:
		static TSharedPtr<FAudioCapturer> Create(const int InSampleRate = 48000, const int InNumChannels = 2, const float InSampleSizeInSeconds = 0.5f);
		virtual ~FAudioCapturer() = default;

		// Mixed audio input will push its audio to an FPatchInputProxy for mixing
		TSharedPtr<FAudioProducer> CreateAudioProducer();
		void					   CreateAudioProducer(Audio::FDeviceId AudioDeviceId);
		void					   RemoveAudioProducer(Audio::FDeviceId AudioDeviceId);

		virtual void PushAudio(const float* AudioData, int32 NumSamples, int32 NumChannels, int32 SampleRate) override;

		/**
		 * This is broadcast each time audio is captured. Tracks should bind to this and push the audio into the track
		 */
		DECLARE_TS_MULTICAST_DELEGATE_FourParams(FOnAudioBuffer, const int16_t*, int32, int32, const int32);
		FOnAudioBuffer OnAudioBuffer;

	protected:
		FAudioCapturer(const int SampleRate = 48000, const int NumChannels = 2, const float SampleSizeInSeconds = 0.5f);

		void OnDebugDumpAudioChanged(IConsoleVariable* Var);
		void OnEnginePreExit();
		void WriteDebugAudio();
		// Allow the mix audio task to call OnAudio.
		friend class FMixAudioTask;
		void OnAudio(const float* AudioData, int32 NumSamples, int32 NumChannels, int32 SampleRate);

	protected:
		TSharedPtr<FAudioPatchMixer>  Mixer;
		TUniqueTaskPtr<FMixAudioTask> MixerTask;

		TMap<Audio::FDeviceId, TSharedPtr<FAudioProducer>> AudioProducers;

		int				  SampleRate;
		int				  NumChannels;
		float			  SampleSizeSeconds;
		Audio::FResampler Resampler;

		Audio::TSampleBuffer<int16_t> DebugDumpAudioBuffer;
	};
} // namespace UE::PixelStreaming2