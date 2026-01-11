// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "EpicRtcTrack.h"
#include "VideoCapturer.h"
#include "VideoSink.h"
#include "RendererInterface.h"

#include "epic_rtc/core/video/video_frame.h"

namespace UE::PixelStreaming2
{
	/**
	 * Video sink class that receives a frame from EpicRtc and passes the frame to all added consumers
	 */
	class PIXELSTREAMING2RTC_API FEpicRtcVideoSink : public FVideoSink, public TEpicRtcTrack<EpicRtcVideoTrackInterface>, public TSharedFromThis<FEpicRtcVideoSink>
	{
	public:
		static TSharedPtr<FEpicRtcVideoSink> Create(TRefCountPtr<EpicRtcVideoTrackInterface> InTrack);
		// Note: destructor will call destroy on any attached video consumers
		virtual ~FEpicRtcVideoSink() = default;

		void OnEpicRtcFrame(const EpicRtcVideoFrame& Frame);

	private:
		FEpicRtcVideoSink(TRefCountPtr<EpicRtcVideoTrackInterface> InTrack);

		void OnFrameCaptured();

		FCriticalSection				  RenderSyncContext;
		FPooledRenderTargetDesc			  RenderTargetDescriptor;
		TRefCountPtr<IPooledRenderTarget> RenderTarget;
		TArray<uint8_t>					  Buffer;
		FTextureRHIRef					  SourceTexture;

		TSharedPtr<FVideoCapturer> VideoCapturer;
	};
} // namespace UE::PixelStreaming2