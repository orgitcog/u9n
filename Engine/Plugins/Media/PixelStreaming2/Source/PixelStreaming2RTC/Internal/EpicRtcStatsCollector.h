// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"

#include "epic_rtc/core/stats.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

namespace UE::PixelStreaming2
{
	class PIXELSTREAMING2RTC_API FEpicRtcStatsCollector : public EpicRtcStatsCollectorCallbackInterface
	{
	public:
		FEpicRtcStatsCollector() = default;
		~FEpicRtcStatsCollector() = default;

		// Begin EpicRtcStatsCollectorCallbackInterface interface
		void			 OnStatsDelivered(const EpicRtcStatsReport& InReport) override;
		// End EpicRtcStatsCollectorCallbackInterface interface

		// Begin EpicRtcRefCountInterface
		EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
		// End EpicRtcStatsCollectorCallbackInterface interface
	public:

		DECLARE_EVENT_TwoParams(FEpicRtcStatsCollector, FOnStatsReady, const FString&, const EpicRtcConnectionStats&);
		FOnStatsReady OnStatsReady;
	};
} // namespace UE::PixelStreaming2