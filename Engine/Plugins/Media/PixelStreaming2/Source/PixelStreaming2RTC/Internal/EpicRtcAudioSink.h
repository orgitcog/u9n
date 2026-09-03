// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioSink.h"
#include "EpicRtcTrack.h"

namespace UE::PixelStreaming2
{
	// Collects audio coming in from EpicRtc and passes into into UE's audio system.
	class PIXELSTREAMING2RTC_API FEpicRtcAudioSink : public FAudioSink, public TEpicRtcTrack<EpicRtcAudioTrackInterface>
	{
	public:
		static TSharedPtr<FEpicRtcAudioSink> Create(TRefCountPtr<EpicRtcAudioTrackInterface> InTrack);
		virtual ~FEpicRtcAudioSink() = default;

	private:
		FEpicRtcAudioSink(TRefCountPtr<EpicRtcAudioTrackInterface> InTrack);
	};
} // namespace UE::PixelStreaming2