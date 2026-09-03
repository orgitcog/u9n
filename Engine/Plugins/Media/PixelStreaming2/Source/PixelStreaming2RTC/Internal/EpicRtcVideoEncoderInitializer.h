// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcVideoCommon.h"
#include "Video/VideoConfig.h"

#include "epic_rtc/core/video/video_encoder.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

namespace UE::PixelStreaming2
{

	class PIXELSTREAMING2RTC_API FEpicRtcVideoEncoderInitializer : public EpicRtcVideoEncoderInitializerInterface
	{
	public:
		FEpicRtcVideoEncoderInitializer() = default;
		virtual ~FEpicRtcVideoEncoderInitializer() = default;

		// Begin EpicRtcVideoEncoderInitializerInterface
		virtual void								 CreateEncoder(EpicRtcVideoCodecInfoInterface* CodecInfo, EpicRtcVideoEncoderInterface** OutEncoder) override;
		virtual EpicRtcStringView					 GetName() override;
		virtual EpicRtcVideoCodecInfoArrayInterface* GetSupportedCodecs() override;
		// End EpicRtcVideoEncoderInitializerInterface

	private:
		TMap<EVideoCodec, TArray<TRefCountPtr<EpicRtcVideoCodecInfoInterface>>> CreateSupportedEncoderMap();

	public:
		// Begin EpicRtcRefCountInterface
		EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
		// End EpicRtcRefCountInterface
	};

} // namespace UE::PixelStreaming2