// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioCapturer.h"

namespace UE::PixelStreaming2
{
	/**
	 * FEpicRtcAudioCapturer overrides the default PushAudio behaviour of the FAudioCapturer in order to
	 * break up the pushed audio into 10ms chunks
	 */
	class PIXELSTREAMING2RTC_API FEpicRtcAudioCapturer : public FAudioCapturer
	{
	public:
		static TSharedPtr<FEpicRtcAudioCapturer> Create();
		virtual ~FEpicRtcAudioCapturer() = default;

		// Override the push audio method as EpicRtc needs the broadcasted audio to be in 10ms chunks
		virtual void PushAudio(const float* AudioData, int32 NumSamples, int32 NumChannels, int32 SampleRate) override;

	protected:
		FEpicRtcAudioCapturer() = default;

	private:
		TArray<int16_t> RecordingBuffer;
	};
} // namespace UE::PixelStreaming2