// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcObserver.h"

#include "epic_rtc/core/data_track_observer.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

#include "EpicRtcDataTrackObserver.generated.h"

UINTERFACE(MinimalAPI)
class UPixelStreaming2DataTrackObserver : public UInterface
{
	GENERATED_BODY()
};

class PIXELSTREAMING2RTC_API IPixelStreaming2DataTrackObserver
{
	GENERATED_BODY()

public:
	virtual void OnDataTrackState(EpicRtcDataTrackInterface* DataTrack, const EpicRtcTrackState State) = 0;
	virtual void OnDataTrackMessage(EpicRtcDataTrackInterface* DataTrack) = 0;
	virtual void OnDataTrackError(EpicRtcDataTrackInterface*, const EpicRtcErrorCode) = 0;
};

namespace UE::PixelStreaming2
{
	class PIXELSTREAMING2RTC_API FEpicRtcDataTrackObserver : public EpicRtcDataTrackObserverInterface
	{
	public:
		FEpicRtcDataTrackObserver(TObserverVariant<IPixelStreaming2DataTrackObserver> UserObserver);
		virtual ~FEpicRtcDataTrackObserver() = default;

	private:
		// Begin EpicRtcDataTrackObserverInterface
		virtual void OnDataTrackState(EpicRtcDataTrackInterface* DataTrack, const EpicRtcTrackState State) override;
		virtual void OnDataTrackMessage(EpicRtcDataTrackInterface* DataTrack) override;
		virtual void OnDataTrackError(EpicRtcDataTrackInterface*, const EpicRtcErrorCode) override;
		// End EpicRtcDataTrackObserverInterface

	public:
		// Begin EpicRtcRefCountInterface
		EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
		// End EpicRtcRefCountInterface

	private:
		TObserverVariant<IPixelStreaming2DataTrackObserver> UserObserver;
	};

} // namespace UE::PixelStreaming2