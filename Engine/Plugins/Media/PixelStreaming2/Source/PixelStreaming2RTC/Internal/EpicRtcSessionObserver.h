// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcObserver.h"

#include "epic_rtc/core/session_observer.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

#include "EpicRtcSessionObserver.generated.h"

UINTERFACE(MinimalAPI)
class UPixelStreaming2SessionObserver : public UInterface
{
	GENERATED_BODY()
};

class PIXELSTREAMING2RTC_API IPixelStreaming2SessionObserver
{
	GENERATED_BODY()

public:
	virtual void OnSessionStateUpdate(const EpicRtcSessionState State) = 0;
	virtual void OnSessionErrorUpdate(const EpicRtcErrorCode Error) = 0;
	virtual void OnSessionRoomsAvailableUpdate(EpicRtcStringArrayInterface* RoomsList) = 0;
};

namespace UE::PixelStreaming2
{
	class PIXELSTREAMING2RTC_API FEpicRtcSessionObserver : public EpicRtcSessionObserverInterface
	{
	public:
		FEpicRtcSessionObserver(TObserverVariant<IPixelStreaming2SessionObserver> UserObserver);
		virtual ~FEpicRtcSessionObserver() = default;

	private:
		// Begin EpicRtcSessionObserver
		virtual void OnSessionStateUpdate(const EpicRtcSessionState State) override;
		virtual void OnSessionErrorUpdate(const EpicRtcErrorCode Error) override;
		virtual void OnSessionRoomsAvailableUpdate(EpicRtcStringArrayInterface* RoomsList) override;
		// End EpicRtcSessionObserver
	public:
		// Begin EpicRtcRefCountInterface
		EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
		// End EpicRtcRefCountInterface
	private:
		TObserverVariant<IPixelStreaming2SessionObserver> UserObserver;
	};

} // namespace UE::PixelStreaming2
