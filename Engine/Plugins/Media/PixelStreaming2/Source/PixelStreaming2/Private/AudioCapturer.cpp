// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioCapturer.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Logging.h"
#include "Misc/CoreDelegates.h"
#include "PixelStreaming2PluginSettings.h"
#include "Sound/SampleBufferIO.h"

namespace UE::PixelStreaming2
{
	FAudioPatchMixer::FAudioPatchMixer(uint8 NumChannels, uint32 SampleRate, float SampleSizeSeconds)
		: NumChannels(NumChannels)
		, SampleRate(SampleRate)
		, SampleSizeSeconds(SampleSizeSeconds)
	{
	}

	uint32 FAudioPatchMixer::GetMaxBufferSize() const
	{
		return NumChannels * SampleRate * SampleSizeSeconds;
	}

	uint8 FAudioPatchMixer::GetNumChannels() const
	{
		return NumChannels;
	}

	uint32 FAudioPatchMixer::GetSampleRate() const
	{
		return SampleRate;
	}

	/***************************************************
	 *
	 ****************************************************/
	FPatchInputProxy::FPatchInputProxy(TSharedPtr<FAudioPatchMixer> InMixer)
		: Mixer(InMixer)
		// We don't want the patch input to handle gain as the capturer handles that at the end
		, PatchInput(Mixer->AddNewInput(Mixer->GetMaxBufferSize(), 1.0f))
		, NumChannels(Mixer->GetNumChannels())
		, SampleRate(Mixer->GetSampleRate())
	{
	}

	FPatchInputProxy::~FPatchInputProxy()
	{
		Mixer->RemovePatch(PatchInput);
	}

	void FPatchInputProxy::PushAudio(const float* AudioData, int32 InNumSamples, int32 InNumChannels, int32 InSampleRate)
	{
		TArray<float> AudioBuffer;

		if (SampleRate != InSampleRate)
		{
			float SampleRateConversionRatio = static_cast<float>(SampleRate) / static_cast<float>(InSampleRate);
			Resampler.Init(Audio::EResamplingMethod::Linear, SampleRateConversionRatio, InNumChannels);

			int32 NumOriginalSamples = InNumSamples / InNumChannels;
			int32 NumConvertedSamples = FMath::CeilToInt(InNumSamples / InNumChannels * SampleRateConversionRatio);
			int32 OutputSamples = INDEX_NONE;
			AudioBuffer.AddZeroed(NumConvertedSamples * InNumChannels);

			// Perform the sample rate conversion
			int32 ErrorCode = Resampler.ProcessAudio(const_cast<float*>(AudioData), NumOriginalSamples, false, AudioBuffer.GetData(), NumConvertedSamples, OutputSamples);
			verifyf(OutputSamples <= NumConvertedSamples, TEXT("OutputSamples > NumConvertedSamples"));
			if (ErrorCode != 0)
			{
				UE_LOG(LogPixelStreaming2, Warning, TEXT("(FAudioInput) Problem occured resampling audio data. Code: %d"), ErrorCode);
				return;
			}
		}
		else
		{
			AudioBuffer.AddZeroed(InNumSamples);
			FMemory::Memcpy(AudioBuffer.GetData(), AudioData, AudioBuffer.Num() * sizeof(float));
		}

		// Note: TSampleBuffer takes in AudioData as float* and internally converts to int16
		Audio::TSampleBuffer<int16_t> Buffer(AudioBuffer.GetData(), AudioBuffer.Num(), InNumChannels, SampleRate);
		if (InNumChannels != NumChannels)
		{
			if (InNumChannels < NumChannels)
			{
				// Up mix by duplicating the mono source to each channel
				TArrayView<int16> SourceBuffer = Buffer.GetArrayView();
				TArray<int16>	  MixedBuffer;
				MixedBuffer.SetNumZeroed(InNumSamples / InNumChannels * NumChannels);
				for (int32 SrcSampleIdx = 0; SrcSampleIdx < Buffer.GetNumSamples(); SrcSampleIdx++)
				{
					int32 DestSampleIdx = SrcSampleIdx * NumChannels;
					for (int32 Channel = 0; Channel < NumChannels; Channel++)
					{
						MixedBuffer[DestSampleIdx + Channel] = SourceBuffer[SrcSampleIdx];
					}
				}

				Buffer.CopyFrom(MixedBuffer, NumChannels, InSampleRate);
			}
			else
			{
				// Down mix using inbuilt method
				Buffer.MixBufferToChannels(NumChannels);
			}
		}

		// Apply gain
		float Gain = UPixelStreaming2PluginSettings::CVarWebRTCAudioGain.GetValueOnAnyThread();
		if (Gain != 1.0f)
		{
			int16* PCMAudio = Buffer.GetArrayView().GetData();

			// multiply audio by gain multiplier
			for (int i = 0; i < Buffer.GetNumSamples(); i++)
			{
				*PCMAudio = FMath::Max(-32768, FMath::Min(32767, *PCMAudio * Gain));
				PCMAudio++;
			}
		}

		Audio::TSampleBuffer<float> BufferToPush(Buffer.GetData(), Buffer.GetNumSamples(), Buffer.GetNumChannels(), Buffer.GetSampleRate());
		PatchInput.PushAudio(BufferToPush.GetData(), BufferToPush.GetNumSamples());
	}

	/***************************************************
	 *
	 ****************************************************/
	FMixAudioTask::FMixAudioTask(FAudioCapturer* Capturer, TSharedPtr<FAudioPatchMixer> Mixer)
		: Capturer(Capturer)
		, Mixer(Mixer)
	{
		MixingBuffer.SetNumUninitialized(Mixer->GetMaxBufferSize());
	}

	void FMixAudioTask::Tick(float DeltaMs)
	{
		if (!Mixer)
		{
			return;
		}

		// 4 samples is the absolute minimum required for mixing
		if (MixingBuffer.Num() < 4)
		{
			return;
		}

		int32 TargetNumSamples = Mixer->MaxNumberOfSamplesThatCanBePopped();
		if (TargetNumSamples < 0)
		{
			return;
		}

		int32 NSamplesPopped = Mixer->PopAudio(MixingBuffer.GetData(), TargetNumSamples, false /* bUseLatestAudio */);
		if (NSamplesPopped == 0)
		{
			return;
		}

		Capturer->OnAudio(MixingBuffer.GetData(), NSamplesPopped, Mixer->GetNumChannels(), Mixer->GetSampleRate());
	}
	const FString& FMixAudioTask::GetName() const
	{
		static FString TaskName = TEXT("MixAudioTask");
		return TaskName;
	}

	/***************************************************
	 *
	 ****************************************************/
	TSharedPtr<FAudioCapturer> FAudioCapturer::Create(const int InSampleRate, const int InNumChannels, const float InSampleSizeInSeconds)
	{
		TSharedPtr<FAudioCapturer> AudioCapturer(new FAudioCapturer(InSampleRate, InNumChannels, InSampleSizeInSeconds));

		FAudioDeviceManagerDelegates::OnAudioDeviceCreated.AddSP(AudioCapturer.ToSharedRef(), &FAudioCapturer::CreateAudioProducer);
		FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddSP(AudioCapturer.ToSharedRef(), &FAudioCapturer::RemoveAudioProducer);

		if (UPixelStreaming2PluginSettings::FDelegates* Delegates = UPixelStreaming2PluginSettings::Delegates())
		{
			Delegates->OnDebugDumpAudioChanged.AddSP(AudioCapturer.ToSharedRef(), &FAudioCapturer::OnDebugDumpAudioChanged);

			TWeakPtr<FAudioCapturer> WeakAudioMixingCapturer = AudioCapturer;
			FCoreDelegates::OnEnginePreExit.AddLambda([WeakAudioMixingCapturer]() {
				if (TSharedPtr<FAudioCapturer> AudioCapturer = WeakAudioMixingCapturer.Pin())
				{
					AudioCapturer->OnEnginePreExit();
				}
			});
		}

		return AudioCapturer;
	}

	FAudioCapturer::FAudioCapturer(const int SampleRate, const int NumChannels, const float NumSampleSizeInSeconds)
		: SampleRate(SampleRate)
		, NumChannels(NumChannels)
		, SampleSizeSeconds(NumSampleSizeInSeconds)
	{
		// subscribe to audio data
		if (!GEngine)
		{
			// No engine. Possibly running editor tests
			return;
		}
		Mixer = MakeShared<FAudioPatchMixer>(NumChannels, SampleRate, SampleSizeSeconds);
		MixerTask = FPixelStreamingTickableTask::Create<FMixAudioTask>(this, Mixer);
		FAudioDeviceManager::Get()->IterateOverAllDevices([this](Audio::FDeviceId AudioDeviceId, FAudioDevice*) {
			CreateAudioProducer(AudioDeviceId);
		});
	}

	// Mixed audio input will push its audio to an FPatchInputProxy for mixing
	TSharedPtr<FAudioProducer> FAudioCapturer::CreateAudioProducer()
	{
		// The lifetimes of audio producers created by the user are the responsibility of the user
		return FAudioProducer::Create(MakeShared<FPatchInputProxy>(Mixer));
	}

	void FAudioCapturer::CreateAudioProducer(Audio::FDeviceId AudioDeviceId)
	{
		// The lifetimes of audio producers created by the engine are our responsibility
		TSharedPtr<FAudioProducer> AudioInput = FAudioProducer::Create(AudioDeviceId, MakeShared<FPatchInputProxy>(Mixer));
		AudioProducers.Add(AudioDeviceId, AudioInput);
	}

	void FAudioCapturer::RemoveAudioProducer(Audio::FDeviceId AudioDeviceId)
	{
		AudioProducers.Remove(AudioDeviceId);
	}

	void FAudioCapturer::OnAudio(const float* AudioData, int32 InNumSamples, int32 InNumChannels, int32 InSampleRate)
	{
		// Note: TSampleBuffer takes in AudioData as float* and internally converts to int16
		Audio::TSampleBuffer<int16_t> Buffer(AudioData, InNumSamples, InNumChannels, SampleRate);

		if (UPixelStreaming2PluginSettings::CVarDebugDumpAudio.GetValueOnAnyThread())
		{
			DebugDumpAudioBuffer.Append(Buffer.GetData(), Buffer.GetNumSamples(), Buffer.GetNumChannels(), Buffer.GetSampleRate());
		}

		PushAudio(AudioData, InNumSamples, InNumChannels, InSampleRate);
	}

	void FAudioCapturer::PushAudio(const float* AudioData, int32 InNumSamples, int32 InNumChannels, int32 InSampleRate)
	{
		Audio::TSampleBuffer<int16_t> Buffer(AudioData, InNumSamples, InNumChannels, SampleRate);

		OnAudioBuffer.Broadcast(Buffer.GetData(), Buffer.GetNumSamples(), Buffer.GetNumChannels(), Buffer.GetSampleRate());
	}

	void FAudioCapturer::OnDebugDumpAudioChanged(IConsoleVariable* Var)
	{
		if (!Var->GetBool())
		{
			WriteDebugAudio();
		}
	}

	void FAudioCapturer::OnEnginePreExit()
	{
		// If engine is exiting but the dump cvar is true, we need to manually trigger a write
		if (UPixelStreaming2PluginSettings::CVarDebugDumpAudio.GetValueOnAnyThread())
		{
			WriteDebugAudio();
		}
	}

	void FAudioCapturer::WriteDebugAudio()
	{
		// Only write audio if we actually have some
		if (DebugDumpAudioBuffer.GetSampleDuration() <= 0.f)
		{
			return;
		}

		Audio::FSoundWavePCMWriter Writer;
		FString					   FilePath = TEXT("");
		Writer.SynchronouslyWriteToWavFile(DebugDumpAudioBuffer, TEXT("PixelStreamingMixedAudio"), TEXT(""), &FilePath);
		UE_LOGFMT(LogPixelStreaming2, Log, "Saving audio sample to: {0}", FilePath);
		DebugDumpAudioBuffer.Reset();
	}
} // namespace UE::PixelStreaming2