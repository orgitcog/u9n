// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelStreaming2RTCModule.h"
#include "EpicRtcConferenceUtils.h"
#include "EpicRtcStatsCollector.h"
#include "EpicRtcStreamer.h"

#include "epic_rtc/core/platform.h"
#include "epic_rtc/plugins/signalling/signalling_type.h"

class UPixelStreaming2Input;
class SWindow;

namespace UE::PixelStreaming2
{
	class FVideoInputBackBuffer;
	class FVideoSourceGroup;
	class FEpicRtcWebsocketFactory;

	/**
	 * This plugin allows the back buffer to be sent as a compressed video across a network.
	 */
	class FPixelStreaming2RTCModule : public IPixelStreaming2RTCModule
	{
	public:
		static FPixelStreaming2RTCModule* GetModule();

		virtual ~FPixelStreaming2RTCModule() = default;

		// Begin IPixelStreaming2RTCModule
		virtual FReadyEvent& OnReady() override;
		virtual bool		 IsReady() override;
		// End IPixelStreaming2RTCModule

		TSharedPtr<class FEpicRtcAudioCapturer>	 GetAudioCapturer();
		TRefCountPtr<EpicRtcConferenceInterface>& GetEpicRtcConference() { return EpicRtcConference; }
		TRefCountPtr<FEpicRtcStatsCollector>&	 GetStatsCollector() { return StatsCollector; }

	private:
		// Begin IModuleInterface
		void StartupModule() override;
		void ShutdownModule() override;
		// End IModuleInterface

		FString GetFieldTrials();
		bool	InitializeEpicRtc();

	private:
		bool							  bModuleReady = false;
		bool							  bStartupCompleted = false;
		static FPixelStreaming2RTCModule* PixelStreaming2Module;

		FReadyEvent		ReadyEvent;
		FDelegateHandle LogStatsHandle;

	private:
		TSharedPtr<class FEpicRtcAudioCapturer>	 AudioMixingCapturer;
		TRefCountPtr<EpicRtcPlatformInterface>	 EpicRtcPlatform;
		TRefCountPtr<EpicRtcConferenceInterface> EpicRtcConference;
		TRefCountPtr<FEpicRtcStatsCollector>	 StatsCollector;

		TRefCountPtr<FEpicRtcWebsocketFactory>	   WebsocketFactory;
		TUniqueTaskPtr<FEpicRtcTickConferenceTask> TickConferenceTask;

		TArray<EpicRtcVideoEncoderInitializerInterface*> EpicRtcVideoEncoderInitializers;
		TArray<EpicRtcVideoDecoderInitializerInterface*> EpicRtcVideoDecoderInitializers;

		static FUtf8String EpicRtcConferenceName;

		TUniquePtr<FRTCStreamerFactory> StreamerFactory;
	};
} // namespace UE::PixelStreaming2
