// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcObserver.h"

#include "epic_rtc/core/video/video_track_observer.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

#include "EpicRtcVideoTrackObserver.generated.h"

UINTERFACE(MinimalAPI)
class UPixelStreaming2VideoTrackObserver : public UInterface
{
	GENERATED_BODY()
};

class PIXELSTREAMING2RTC_API IPixelStreaming2VideoTrackObserver
{
	GENERATED_BODY()

public:
	virtual void		OnVideoTrackMuted(EpicRtcVideoTrackInterface* VideoTrack, EpicRtcBool bIsMuted) = 0;
	virtual void		OnVideoTrackFrame(EpicRtcVideoTrackInterface* VideoTrack, const EpicRtcVideoFrame& Frame) = 0;
	virtual void		OnVideoTrackRemoved(EpicRtcVideoTrackInterface* VideoTrack) = 0;
	virtual void		OnVideoTrackState(EpicRtcVideoTrackInterface* VideoTrack, const EpicRtcTrackState State) = 0;
	virtual void		OnVideoTrackEncodedFrame(EpicRtcVideoTrackInterface*, const EpicRtcEncodedVideoFrame&) = 0;
	virtual EpicRtcBool Enabled() const = 0;
};

namespace UE::PixelStreaming2
{
	class PIXELSTREAMING2RTC_API FEpicRtcVideoTrackObserver : public EpicRtcVideoTrackObserverInterface
	{
	public:
		FEpicRtcVideoTrackObserver(TObserverVariant<IPixelStreaming2VideoTrackObserver> UserObserver);
		virtual ~FEpicRtcVideoTrackObserver() = default;

	private:
		// Begin EpicRtcVideoTrackObserverInterface
		virtual void		OnVideoTrackMuted(EpicRtcVideoTrackInterface* VideoTrack, EpicRtcBool bIsMuted) override;
		virtual void		OnVideoTrackFrame(EpicRtcVideoTrackInterface* VideoTrack, const EpicRtcVideoFrame& Frame) override;
		virtual void		OnVideoTrackRemoved(EpicRtcVideoTrackInterface* VideoTrack) override;
		virtual void		OnVideoTrackState(EpicRtcVideoTrackInterface* VideoTrack, const EpicRtcTrackState State) override;
		virtual void		OnVideoTrackEncodedFrame(EpicRtcVideoTrackInterface*, const EpicRtcEncodedVideoFrame&) override;
		virtual EpicRtcBool Enabled() const override;
		// End EpicRtcVideoTrackObserverInterface

	public:
		// Begin EpicRtcRefCountInterface
		EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
		// End EpicRtcRefCountInterface

	private:
		TObserverVariant<IPixelStreaming2VideoTrackObserver> UserObserver;
	};

} // namespace UE::PixelStreaming2