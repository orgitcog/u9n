// Copyright Epic Games, Inc. All Rights Reserved.

#include "EpicRtcAudioCapturer.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Logging.h"
#include "Misc/CoreDelegates.h"
#include "PixelStreaming2PluginSettings.h"
#include "Sound/SampleBufferIO.h"

namespace UE::PixelStreaming2
{
	TSharedPtr<FEpicRtcAudioCapturer> FEpicRtcAudioCapturer::Create()
	{
		TSharedPtr<FEpicRtcAudioCapturer> AudioMixingCapturer(new FEpicRtcAudioCapturer());

		FAudioDeviceManagerDelegates::OnAudioDeviceCreated.AddSP(AudioMixingCapturer.ToSharedRef(), &FEpicRtcAudioCapturer::CreateAudioProducer);
		FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddSP(AudioMixingCapturer.ToSharedRef(), &FEpicRtcAudioCapturer::RemoveAudioProducer);

		if (UPixelStreaming2PluginSettings::FDelegates* Delegates = UPixelStreaming2PluginSettings::Delegates())
		{
			Delegates->OnDebugDumpAudioChanged.AddSP(AudioMixingCapturer.ToSharedRef(), &FEpicRtcAudioCapturer::OnDebugDumpAudioChanged);

			TWeakPtr<FEpicRtcAudioCapturer> WeakAudioMixingCapturer = AudioMixingCapturer;
			FCoreDelegates::OnEnginePreExit.AddLambda([WeakAudioMixingCapturer]() {
				if (TSharedPtr<FEpicRtcAudioCapturer> AudioMixingCapturer = WeakAudioMixingCapturer.Pin())
				{
					AudioMixingCapturer->OnEnginePreExit();
				}
			});
		}

		return AudioMixingCapturer;
	}

	void FEpicRtcAudioCapturer::PushAudio(const float* AudioData, int32 InNumSamples, int32 InNumChannels, const int32 InSampleRate)
	{
		Audio::TSampleBuffer<int16_t> Buffer(AudioData, InNumSamples, InNumChannels, SampleRate);
		RecordingBuffer.Append(Buffer.GetData(), Buffer.GetNumSamples());

		const int32	 SamplesPer10Ms = NumChannels * SampleRate * 0.01f;
		const size_t BytesPerFrame = NumChannels * sizeof(int16_t);

		// Feed in 10ms chunks
		while (RecordingBuffer.Num() > SamplesPer10Ms)
		{
			OnAudioBuffer.Broadcast(RecordingBuffer.GetData(), SamplesPer10Ms, NumChannels, SampleRate);

			// Remove 10ms of samples from the recording buffer now it is submitted
			RecordingBuffer.RemoveAt(0, SamplesPer10Ms, EAllowShrinking::No);
		}
	}
} // namespace UE::PixelStreaming2