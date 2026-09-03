// Copyright Epic Games, Inc. All Rights Reserved.

#include "EpicRtcAudioTrackObserver.h"

namespace UE::PixelStreaming2
{
	FEpicRtcAudioTrackObserver::FEpicRtcAudioTrackObserver(TObserverVariant<IPixelStreaming2AudioTrackObserver> UserObserver)
		: UserObserver(UserObserver)
	{
	}

	void FEpicRtcAudioTrackObserver::OnAudioTrackMuted(EpicRtcAudioTrackInterface* AudioTrack, EpicRtcBool bIsMuted)
	{
		if (UserObserver)
		{
			UserObserver->OnAudioTrackMuted(AudioTrack, bIsMuted);
		}
	}

	void FEpicRtcAudioTrackObserver::OnAudioTrackFrame(EpicRtcAudioTrackInterface* AudioTrack, const EpicRtcAudioFrame& Frame)
	{
		if (UserObserver)
		{
			UserObserver->OnAudioTrackFrame(AudioTrack, Frame);
		}
	}

	void FEpicRtcAudioTrackObserver::OnAudioTrackRemoved(EpicRtcAudioTrackInterface* AudioTrack)
	{
		if (UserObserver)
		{
			UserObserver->OnAudioTrackRemoved(AudioTrack);
		}
	}

	void FEpicRtcAudioTrackObserver::OnAudioTrackState(EpicRtcAudioTrackInterface* AudioTrack, const EpicRtcTrackState State)
	{
		if (UserObserver)
		{
			UserObserver->OnAudioTrackState(AudioTrack, State);
		}
	}

} // namespace UE::PixelStreaming2