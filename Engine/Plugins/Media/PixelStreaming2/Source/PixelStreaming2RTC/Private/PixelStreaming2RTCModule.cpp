// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreaming2RTCModule.h"

#include "UtilsCoder.h"
#include "CoreMinimal.h"
#include "UtilsCore.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "IPixelStreaming2Module.h"
#include "IPixelStreaming2InputModule.h"
#include "Logging.h"
#include "Modules/ModuleManager.h"
#include "PixelStreaming2Delegates.h"
#include "PixelStreaming2Utils.h"
#include "PixelStreaming2PluginSettings.h"
#include "Slate/SceneViewport.h"
#include "EpicRtcStreamer.h"
#include "UObject/UObjectIterator.h"
#include "UtilsCommon.h"

#if PLATFORM_LINUX
	#include "CudaModule.h"
#endif

#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
	#include <VersionHelpers.h>
THIRD_PARTY_INCLUDES_END
	#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "AudioDeviceManager.h"
#include "RenderingThread.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RendererInterface.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/GameModeBase.h"
#include "Dom/JsonObject.h"
#include "Misc/App.h"
#include "Misc/MessageDialog.h"
#include "Async/Async.h"
#include "Engine/Engine.h"
#include "WebSocketsModule.h"

#include "Video/Encoders/Configs/VideoEncoderConfigH264.h"
#include "Video/Encoders/Configs/VideoEncoderConfigAV1.h"

#if !UE_BUILD_SHIPPING
	#include "DrawDebugHelpers.h"
#endif

#include "VideoProducerBackBuffer.h"
#include "VideoProducerMediaCapture.h"
#include "Engine/GameEngine.h"
#include "Stats.h"
#include "UtilsString.h"

#include "EpicRtcAllocator.h"
#include "EpicRtcAudioCapturer.h"
#include "EpicRtcLogging.h"
#include "EpicRtcVideoEncoderInitializer.h"
#include "EpicRtcVideoDecoderInitializer.h"
#include "EpicRtcWebsocketFactory.h"

namespace UE::PixelStreaming2
{
	FPixelStreaming2RTCModule* FPixelStreaming2RTCModule::PixelStreaming2Module = nullptr;

	FUtf8String FPixelStreaming2RTCModule::EpicRtcConferenceName("pixel_streaming_conference_instance");

	/**
	 * Stats logger - as turned on/off by CVarPixelStreaming2LogStats
	 */
	void ConsumeStat(FString PlayerId, FName StatName, float StatValue)
	{
		UE_LOGFMT(LogPixelStreaming2RTC, Log, "[{0}]({1}) = {2}", PlayerId, StatName.ToString(), StatValue);
	}

	/**
	 * IModuleInterface implementation
	 */
	void FPixelStreaming2RTCModule::StartupModule()
	{
#if UE_SERVER
		// Hack to no-op the rest of the module so Blueprints can still work
		return;
#endif
		if (!IsStreamingSupported())
		{
			return;
		}

		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		const ERHIInterfaceType RHIType = GDynamicRHI ? RHIGetInterfaceType() : ERHIInterfaceType::Hidden;
		// only D3D11/D3D12/Vulkan is supported
		if (!(RHIType == ERHIInterfaceType::D3D11 || RHIType == ERHIInterfaceType::D3D12 || RHIType == ERHIInterfaceType::Vulkan || RHIType == ERHIInterfaceType::Metal))
		{
#if !WITH_DEV_AUTOMATION_TESTS
			UE_LOG(LogPixelStreaming2RTC, Warning, TEXT("Only D3D11/D3D12/Vulkan/Metal Dynamic RHI is supported. Detected %s"), GDynamicRHI != nullptr ? GDynamicRHI->GetName() : TEXT("[null]"));
#endif
			return;
		}

		FString LogFilterString = UPixelStreaming2PluginSettings::CVarEpicRtcLogFilter.GetValueOnAnyThread() + TEXT("//\\bConference::Tick. Ticking audio (?:too|to) late\\b");
		UPixelStreaming2PluginSettings::CVarEpicRtcLogFilter->Set(*LogFilterString, ECVF_SetByHotfix);

		// By calling InitDefaultStreamer post engine init we can use pixel streaming in standalone editor mode
		IPixelStreaming2Module::Get().OnReady().AddLambda([this](IPixelStreaming2Module& CoreModule) {
			// Need to initialize after other modules have initialized such as NVCodec.
			if (!InitializeEpicRtc())
			{
				return;
			}

			if (!ensure(GEngine != nullptr))
			{
				return;
			}

			StreamerFactory.Reset(new FRTCStreamerFactory(EpicRtcConference));

			// Ensure we have ImageWrapper loaded, used in Freezeframes
			verify(FModuleManager::Get().LoadModule(FName("ImageWrapper")));

			bModuleReady = true;
			ReadyEvent.Broadcast(*this);
		});

		FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets");

		// Call these to initialize their singletons
		FStats::Get();

		if (UPixelStreaming2PluginSettings::FDelegates* Delegates = UPixelStreaming2PluginSettings::Delegates())
		{
			Delegates->OnLogStatsChanged.AddLambda([this](IConsoleVariable* Var) {
				bool					   bLogStats = Var->GetBool();
				UPixelStreaming2Delegates* Delegates = UPixelStreaming2Delegates::Get();
				if (!Delegates)
				{
					return;
				}
				if (bLogStats)
				{
					LogStatsHandle = Delegates->OnStatChangedNative.AddStatic(&ConsumeStat);
				}
				else
				{
					Delegates->OnStatChangedNative.Remove(LogStatsHandle);
				}
			});

			Delegates->OnWebRTCFpsChanged.AddLambda([](IConsoleVariable*) {
				IPixelStreaming2Module::Get().ForEachStreamer([](TSharedPtr<IPixelStreaming2Streamer> Streamer) {
					Streamer->RefreshStreamBitrate();
				});
			});

			Delegates->OnWebRTCBitrateChanged.AddLambda([](IConsoleVariable*) {
				IPixelStreaming2Module::Get().ForEachStreamer([](TSharedPtr<IPixelStreaming2Streamer> Streamer) {
					Streamer->RefreshStreamBitrate();
				});
			});
			Delegates->OnWebRTCDisableStatsChanged.AddLambda([this](IConsoleVariable* Var) {
				if (EpicRtcConference)
				{
					if (Var->GetBool())
					{
						EpicRtcConference->DisableStats();
					}
					else
					{
						EpicRtcConference->EnableStats();
					}
				}
			});
		}

		bStartupCompleted = true;
	}

	void FPixelStreaming2RTCModule::ShutdownModule()
	{
		if (!IsStreamingSupported())
		{
			return;
		}

		if (!bStartupCompleted)
		{
			return;
		}

		AudioMixingCapturer.Reset();

		TickConferenceTask.Reset();
		StreamerFactory.Reset();

		if (!EpicRtcPlatform)
		{
			UE_LOGFMT(LogPixelStreaming2RTC, Error, "EpicRtcPlatform does not exist during shutdown when it is expected to exist");
		}
		else
		{
			EpicRtcPlatform->ReleaseConference(ToEpicRtcStringView(EpicRtcConferenceName));
		}

		bStartupCompleted = false;
	}

	/**
	 * End IModuleInterface implementation
	 */

	FPixelStreaming2RTCModule* FPixelStreaming2RTCModule::GetModule()
	{
		if (!PixelStreaming2Module)
		{
			PixelStreaming2Module = FModuleManager::Get().LoadModulePtr<FPixelStreaming2RTCModule>("PixelStreaming2RTC");
		}

		return PixelStreaming2Module;
	}

	/**
	 * IPixelStreaming2RTCModule implementation
	 */
	IPixelStreaming2RTCModule::FReadyEvent& FPixelStreaming2RTCModule::OnReady()
	{
		return ReadyEvent;
	}

	bool FPixelStreaming2RTCModule::IsReady()
	{
		return bModuleReady;
	}
	/**
	 * End IPixelStreaming2RTCModule implementation
	 */

	TSharedPtr<FEpicRtcAudioCapturer> FPixelStreaming2RTCModule::GetAudioCapturer()
	{
		if (!AudioMixingCapturer)
		{
			AudioMixingCapturer = FEpicRtcAudioCapturer::Create();
		}

		return AudioMixingCapturer;
	}

	FString FPixelStreaming2RTCModule::GetFieldTrials()
	{
		FString FieldTrials = UPixelStreaming2PluginSettings::CVarWebRTCFieldTrials.GetValueOnAnyThread();

		// Set the WebRTC-FrameDropper/Disabled/ if the CVar is set
		if (UPixelStreaming2PluginSettings::CVarWebRTCDisableFrameDropper.GetValueOnAnyThread())
		{
			FieldTrials += TEXT("WebRTC-FrameDropper/Disabled/");
		}

		if (UPixelStreaming2PluginSettings::CVarWebRTCEnableFlexFec.GetValueOnAnyThread())
		{
			FieldTrials += TEXT("WebRTC-FlexFEC-03-Advertised/Enabled/WebRTC-FlexFEC-03/Enabled/");
		}

		// Parse "WebRTC-Video-Pacing/" field trial
		{
			float PacingFactor = UPixelStreaming2PluginSettings::CVarWebRTCVideoPacingFactor.GetValueOnAnyThread();
			float PacingMaxDelayMs = UPixelStreaming2PluginSettings::CVarWebRTCVideoPacingMaxDelay.GetValueOnAnyThread();

			if (PacingFactor >= 0.0f || PacingMaxDelayMs >= 0.0f)
			{
				FString VideoPacingFieldTrialStr = TEXT("WebRTC-Video-Pacing/");
				bool	bHasPacingFactor = PacingFactor >= 0.0f;
				if (bHasPacingFactor)
				{
					VideoPacingFieldTrialStr += FString::Printf(TEXT("factor:%.1f"), PacingFactor);
				}
				bool bHasMaxDelay = PacingMaxDelayMs >= 0.0f;
				if (bHasMaxDelay)
				{
					VideoPacingFieldTrialStr += bHasPacingFactor ? TEXT(",") : TEXT("");
					VideoPacingFieldTrialStr += FString::Printf(TEXT("max_delay:%.0f"), PacingMaxDelayMs);
				}
				VideoPacingFieldTrialStr += TEXT("/");
				FieldTrials += VideoPacingFieldTrialStr;
			}
		}

		return FieldTrials;
	}

	bool FPixelStreaming2RTCModule::InitializeEpicRtc()
	{
		EpicRtcVideoEncoderInitializers = { new FEpicRtcVideoEncoderInitializer() };
		EpicRtcVideoDecoderInitializers = { new FEpicRtcVideoDecoderInitializer() };

		EpicRtcPlatformConfig PlatformConfig{
			._memory = new FEpicRtcAllocator()
		};

		EpicRtcErrorCode Result = GetOrCreatePlatform(PlatformConfig, EpicRtcPlatform.GetInitReference());
		if (Result != EpicRtcErrorCode::Ok && Result != EpicRtcErrorCode::FoundExistingPlatform)
		{
			UE_LOG(LogPixelStreaming2RTC, Warning, TEXT("Unable to create EpicRtc Platform. GetOrCreatePlatform returned %s"), *ToString(Result));
			return false;
		}

		FUtf8String EpicRtcFieldTrials(GetFieldTrials());

		WebsocketFactory = MakeRefCount<FEpicRtcWebsocketFactory>();

		StatsCollector = MakeRefCount<FEpicRtcStatsCollector>();

		// clang-format off
		EpicRtcConfig ConferenceConfig = {
			._websocketFactory = WebsocketFactory.GetReference(),
			._signallingType = EpicRtcSignallingType::PixelStreaming,
			._signingPlugin = nullptr,
			._migrationPlugin = nullptr,
			._audioDevicePlugin = nullptr,
			._audioConfig = {
				._tickAdm = true,
				._audioEncoderInitializers = {}, // Not needed because we use the inbuilt audio codecs
				._audioDecoderInitializers = {}, // Not needed because we use the inbuilt audio codecs
				._enableBuiltInAudioCodecs = true,
			},
			._videoConfig = {
				._videoEncoderInitializers = {
					._ptr = const_cast<const EpicRtcVideoEncoderInitializerInterface**>(EpicRtcVideoEncoderInitializers.GetData()),
					._size = (uint64_t)EpicRtcVideoEncoderInitializers.Num()
				},
				._videoDecoderInitializers = {
					._ptr = const_cast<const EpicRtcVideoDecoderInitializerInterface**>(EpicRtcVideoDecoderInitializers.GetData()),
					._size = (uint64_t)EpicRtcVideoDecoderInitializers.Num()
				},
				._enableBuiltInVideoCodecs = false
			},
			._fieldTrials = {
				._fieldTrials = ToEpicRtcStringView(EpicRtcFieldTrials),
				._isGlobal = 0
			},
			._logging = {
				._logger = new FEpicRtcLogsRedirector(MakeShared<FEpicRtcLogFilter>()),
#if !NO_LOGGING // When building WITH_SHIPPING by default .GetVerbosity() does not exist
				._level = UnrealLogToEpicRtcCategoryMap[LogPixelStreaming2EpicRtc.GetVerbosity()],
				._levelWebRtc = UnrealLogToEpicRtcCategoryMap[LogPixelStreaming2WebRtc.GetVerbosity()]
#endif
			},
			._stats = {
				._statsCollectorCallback = StatsCollector.GetReference(),
				._statsCollectorInterval = 1000,
				._jsonFormatOnly = false
			}
		};
		// clang-format on

		Result = EpicRtcPlatform->CreateConference(ToEpicRtcStringView(EpicRtcConferenceName), ConferenceConfig, EpicRtcConference.GetInitReference());
		if (Result != EpicRtcErrorCode::Ok)
		{
			UE_LOG(LogPixelStreaming2RTC, Warning, TEXT("Unable to create EpicRtc Conference: CreateConference returned %s"), *ToString(Result));
			return false;
		}

		TickConferenceTask = FPixelStreamingTickableTask::Create<FEpicRtcTickConferenceTask>(EpicRtcConference, TEXT("PixelStreaming2Module TickConferenceTask"));

		return true;
	}

	/**
	 * End own methods
	 */
} // namespace UE::PixelStreaming2

IMPLEMENT_MODULE(UE::PixelStreaming2::FPixelStreaming2RTCModule, PixelStreaming2RTC)
