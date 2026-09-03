// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcObserver.h"

#include "epic_rtc/core/audio/audio_track_observer.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

class IPixelStreaming2AudioTrackObserver;

namespace UE::PixelStreaming2
{		
	class PIXELSTREAMING2RTC_API FEpicRtcAudioTrackObserverFactory : public EpicRtcAudioTrackObserverFactoryInterface
	{
	public:
		FEpicRtcAudioTrackObserverFactory(TObserverVariant<IPixelStreaming2AudioTrackObserver> UserObserver);
		virtual ~FEpicRtcAudioTrackObserverFactory() = default;

	public:
		// Begin EpicRtcAudioTrackObserverFactoryInterface
		virtual EpicRtcErrorCode CreateAudioTrackObserver(const EpicRtcStringView ParticipantId, const EpicRtcStringView AudioTrackId, EpicRtcAudioTrackObserverInterface** OutAudioTrackObserver) override;
		// End EpicRtcAudioTrackObserverFactoryInterface

	public:
		// Begin EpicRtcRefCountInterface
		EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
		// End EpicRtcRefCountInterface

	private:
		TObserverVariant<IPixelStreaming2AudioTrackObserver> UserObserver;
	};

} // namespace UE::PixelStreaming2