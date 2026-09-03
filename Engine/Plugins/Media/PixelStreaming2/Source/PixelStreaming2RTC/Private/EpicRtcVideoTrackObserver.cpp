// Copyright Epic Games, Inc. All Rights Reserved.

#include "EpicRtcVideoTrackObserver.h"

namespace UE::PixelStreaming2
{

	FEpicRtcVideoTrackObserver::FEpicRtcVideoTrackObserver(TObserverVariant<IPixelStreaming2VideoTrackObserver> UserObserver)
		: UserObserver(UserObserver)
	{
	}

	void FEpicRtcVideoTrackObserver::OnVideoTrackMuted(EpicRtcVideoTrackInterface* VideoTrack, EpicRtcBool bIsMuted)
	{
		if (UserObserver)
		{
			return UserObserver->OnVideoTrackMuted(VideoTrack, bIsMuted);
		}
	}

	void FEpicRtcVideoTrackObserver::OnVideoTrackFrame(EpicRtcVideoTrackInterface* VideoTrack, const EpicRtcVideoFrame& Frame)
	{
		if (UserObserver)
		{
			return UserObserver->OnVideoTrackFrame(VideoTrack, Frame);
		}
	}

	void FEpicRtcVideoTrackObserver::OnVideoTrackRemoved(EpicRtcVideoTrackInterface* VideoTrack)
	{
		if (UserObserver)
		{
			return UserObserver->OnVideoTrackRemoved(VideoTrack);
		}
	}

	void FEpicRtcVideoTrackObserver::OnVideoTrackState(EpicRtcVideoTrackInterface* VideoTrack, const EpicRtcTrackState State)
	{
		if (UserObserver)
		{
			return UserObserver->OnVideoTrackState(VideoTrack, State);
		}
	}

	void FEpicRtcVideoTrackObserver::OnVideoTrackEncodedFrame(EpicRtcVideoTrackInterface* VideoTrack, const EpicRtcEncodedVideoFrame& EncodedFrame)
	{
		if (UserObserver)
		{
			return UserObserver->OnVideoTrackEncodedFrame(VideoTrack, EncodedFrame);
		}
	}

	EpicRtcBool FEpicRtcVideoTrackObserver::Enabled() const
	{
		if (UserObserver)
		{
			return UserObserver->Enabled();
		}
		return false;
	}

} // namespace UE::PixelStreaming2