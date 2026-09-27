// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcWebsocket.h"

#include "epic_rtc/plugins/signalling/websocket.h"
#include "epic_rtc/plugins/signalling/websocket_factory.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

namespace UE::PixelStreaming2
{
	class PIXELSTREAMING2RTC_API FEpicRtcWebsocketFactory : public EpicRtcWebsocketFactoryInterface
	{
	public:
		FEpicRtcWebsocketFactory(bool bInSendKeepAlive = true, TFunction<void(void)> OnMaxReconnectAttemptsExceeded = []() {})
			: bSendKeepAlive(bInSendKeepAlive)
			, OnMaxReconnectAttemptsExceeded(OnMaxReconnectAttemptsExceeded)
		{
		}
		virtual ~FEpicRtcWebsocketFactory() = default;

		virtual EpicRtcErrorCode CreateWebsocket(EpicRtcWebsocketInterface** OutWebsocket) override;

	public:
		// Begin EpicRtcRefCountInterface
		EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
		// End EpicRtcRefCountInterface

	private:
		bool bSendKeepAlive;

		TFunction<void(void)> OnMaxReconnectAttemptsExceeded;
	};

} // namespace UE::PixelStreaming2
