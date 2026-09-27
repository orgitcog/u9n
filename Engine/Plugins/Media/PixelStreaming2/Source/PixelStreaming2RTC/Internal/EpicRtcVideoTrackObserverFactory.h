// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcObserver.h"

#include "epic_rtc/core/video/video_track_observer.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

class IPixelStreaming2VideoTrackObserver;

namespace UE::PixelStreaming2
{	
	class PIXELSTREAMING2RTC_API FEpicRtcVideoTrackObserverFactory : public EpicRtcVideoTrackObserverFactoryInterface
	{
	public:
		FEpicRtcVideoTrackObserverFactory(TObserverVariant<IPixelStreaming2VideoTrackObserver> UserObserver);
		virtual ~FEpicRtcVideoTrackObserverFactory() = default;

	public:
		// Begin EpicRtcVideoTrackObserverFactoryInterface
		virtual EpicRtcErrorCode CreateVideoTrackObserver(const EpicRtcStringView ParticipantId, const EpicRtcStringView VideoTrackId, EpicRtcVideoTrackObserverInterface** OutVideoTrackObserver) override;
		// End EpicRtcVideoTrackObserverFactoryInterface

	public:
		// Begin EpicRtcRefCountInterface
		EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
		// End EpicRtcRefCountInterface

	private:
		TObserverVariant<IPixelStreaming2VideoTrackObserver> UserObserver;
	};

} // namespace UE::PixelStreaming2