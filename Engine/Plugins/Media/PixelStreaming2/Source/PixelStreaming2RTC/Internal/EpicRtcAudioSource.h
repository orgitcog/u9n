// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioSource.h"
#include "EpicRtcTrack.h"
#include "Templates/SharedPointer.h"

#include "epic_rtc/core/audio/audio_track.h"

namespace UE::PixelStreaming2
{
	class PIXELSTREAMING2RTC_API FEpicRtcAudioSource : public FAudioSource, public TEpicRtcTrack<EpicRtcAudioTrackInterface>
	{
	public:
		static TSharedPtr<FEpicRtcAudioSource> Create(TRefCountPtr<EpicRtcAudioTrackInterface> InTrack, TSharedPtr<class FEpicRtcAudioCapturer> InCapturer);

		virtual void OnAudioBuffer(const int16_t* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate);

	private:
		FEpicRtcAudioSource(TRefCountPtr<EpicRtcAudioTrackInterface> InTrack);
	};
} // namespace UE::PixelStreaming2