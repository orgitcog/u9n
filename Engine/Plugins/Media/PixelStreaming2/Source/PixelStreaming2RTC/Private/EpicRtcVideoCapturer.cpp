// Copyright Epic Games, Inc. All Rights Reserved.

#include "EpicRtcVideoCapturer.h"

#include "EpicRtcVideoBufferMultiFormat.h"
#include "PixelStreaming2PluginSettings.h"

namespace UE::PixelStreaming2
{
	TSharedPtr<FEpicRtcVideoCapturer> FEpicRtcVideoCapturer::Create(TSharedPtr<FVideoProducer> InVideoProducer)
	{
		TSharedPtr<FEpicRtcVideoCapturer> VideoCapturer = TSharedPtr<FEpicRtcVideoCapturer>(new FEpicRtcVideoCapturer(InVideoProducer));
		if (InVideoProducer)
		{
			VideoCapturer->FramePushedHandle = InVideoProducer->OnFramePushed.AddSP(VideoCapturer.ToSharedRef(), &FEpicRtcVideoCapturer::OnFrame);
		}

		if (UPixelStreaming2PluginSettings::FDelegates* Delegates = UPixelStreaming2PluginSettings::Delegates())
		{
			Delegates->OnSimulcastEnabledChanged.AddSP(VideoCapturer.ToSharedRef(), &FEpicRtcVideoCapturer::OnSimulcastEnabledChanged);
			Delegates->OnCaptureUseFenceChanged.AddSP(VideoCapturer.ToSharedRef(), &FEpicRtcVideoCapturer::OnCaptureUseFenceChanged);
			Delegates->OnUseMediaCaptureChanged.AddSP(VideoCapturer.ToSharedRef(), &FEpicRtcVideoCapturer::OnUseMediaCaptureChanged);
		}
		return VideoCapturer;
	}

	FEpicRtcVideoCapturer::FEpicRtcVideoCapturer(TSharedPtr<FVideoProducer> VideoProducer)
		: FVideoCapturer(VideoProducer)
	{
		CreateFrameCapturer();
	}

	TRefCountPtr<EpicRtcVideoBufferInterface> FEpicRtcVideoCapturer::GetFrameBuffer()
	{
		return new FEpicRtcVideoBufferMultiFormatLayered(FrameCapturer);
	}
} // namespace UE::PixelStreaming2