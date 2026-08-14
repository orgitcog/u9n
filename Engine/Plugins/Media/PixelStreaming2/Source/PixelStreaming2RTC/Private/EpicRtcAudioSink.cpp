// Copyright Epic Games, Inc. All Rights Reserved.

#include "EpicRtcAudioSink.h"

namespace UE::PixelStreaming2
{
	TSharedPtr<FEpicRtcAudioSink> FEpicRtcAudioSink::Create(TRefCountPtr<EpicRtcAudioTrackInterface> InTrack)
	{
		return TSharedPtr<FEpicRtcAudioSink>(new FEpicRtcAudioSink(InTrack));
	}

	FEpicRtcAudioSink::FEpicRtcAudioSink(TRefCountPtr<EpicRtcAudioTrackInterface> InTrack)
		: TEpicRtcTrack(InTrack)
	{
	}
} // namespace UE::PixelStreaming2