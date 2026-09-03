// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VideoCapturer.h"

#include "epic_rtc/core/video/video_buffer.h"

namespace UE::PixelStreaming2
{
	class PIXELSTREAMING2RTC_API FEpicRtcVideoCapturer : public FVideoCapturer
	{
	public:
		static TSharedPtr<FEpicRtcVideoCapturer> Create(TSharedPtr<FVideoProducer> VideoProducer = nullptr);
		virtual ~FEpicRtcVideoCapturer() = default;

		TRefCountPtr<EpicRtcVideoBufferInterface> GetFrameBuffer();

	private:
		FEpicRtcVideoCapturer(TSharedPtr<FVideoProducer> VideoProducer);
	};
} // namespace UE::PixelStreaming2
