// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcVideoCommon.h"
#include "Video/VideoConfig.h"

#include "epic_rtc/core/video/video_decoder.h"
#include "epic_rtc_helper/memory/ref_count_impl_helper.h"

namespace UE::PixelStreaming2
{

	class PIXELSTREAMING2RTC_API FEpicRtcVideoDecoderInitializer : public EpicRtcVideoDecoderInitializerInterface
	{
	public:
		FEpicRtcVideoDecoderInitializer() = default;
		virtual ~FEpicRtcVideoDecoderInitializer() = default;

		// Begin EpicRtcVideoDecoderInitializerInterface
		virtual void								 CreateDecoder(EpicRtcVideoCodecInfoInterface* CodecInfo, EpicRtcVideoDecoderInterface** OutDecoder) override;
		virtual EpicRtcStringView					 GetName() override;
		virtual EpicRtcVideoCodecInfoArrayInterface* GetSupportedCodecs() override;
		// End EpicRtcVideoDecoderInitializerInterface

	private:
		TMap<EVideoCodec, TArray<TRefCountPtr<EpicRtcVideoCodecInfoInterface>>> CreateSupportedDecoderMap();

	public:
		// Begin EpicRtcRefCountInterface
		EPICRTC_REFCOUNT_INTERFACE_IN_PLACE
		// End EpicRtcRefCountInterface
	};

} // namespace UE::PixelStreaming2