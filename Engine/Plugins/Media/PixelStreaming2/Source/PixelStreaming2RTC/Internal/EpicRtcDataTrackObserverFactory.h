// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcObserver.h"

#include "epic_rtc/core/data_track_observer.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

class IPixelStreaming2DataTrackObserver;

namespace UE::PixelStreaming2
{
	class PIXELSTREAMING2RTC_API FEpicRtcDataTrackObserverFactory : public EpicRtcDataTrackObserverFactoryInterface
	{
	public:
		FEpicRtcDataTrackObserverFactory(TObserverVariant<IPixelStreaming2DataTrackObserver> UserObserver);
		virtual ~FEpicRtcDataTrackObserverFactory() = default;

	public:
		// Begin EpicRtcDataTrackObserverFactoryInterface
		virtual EpicRtcErrorCode CreateDataTrackObserver(const EpicRtcStringView ParticipantId, const EpicRtcStringView DataTrackId, EpicRtcDataTrackObserverInterface** OutDataTrackObserver) override;
		// End EpicRtcDataTrackObserverFactoryInterface

	public:
		// Begin EpicRtcRefCountInterface
		EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
		// End EpicRtcRefCountInterface

	private:
		TObserverVariant<IPixelStreaming2DataTrackObserver> UserObserver;
	};

} // namespace UE::PixelStreaming2