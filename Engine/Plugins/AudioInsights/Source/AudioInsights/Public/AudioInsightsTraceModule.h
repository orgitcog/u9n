// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Containers/Ticker.h"
#include "IAudioInsightsTraceModule.h"
#include "ITraceController.h"
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"
#include "Templates/SharedPointer.h"
#include "Trace/StoreClient.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"

namespace UE::Audio::Insights
{
	class FRewindDebugger : public IRewindDebuggerRuntimeExtension
	{
	public:
		virtual void RecordingStarted() override;
	};

	class AUDIOINSIGHTS_API FTraceModule : public IAudioInsightsTraceModule
	{
	public:
		FTraceModule();
		virtual ~FTraceModule() = default;

		//~ Begin TraceServices::IModule interface
		virtual void GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo) override;
		virtual void OnAnalysisBegin(TraceServices::IAnalysisSession& Session) override;
		virtual void GetLoggers(TArray<const TCHAR *>& OutLoggers) override;
		virtual void GenerateReports(const TraceServices::IAnalysisSession& Session, const TCHAR* CmdLine, const TCHAR* OutputDirectory) override;
		virtual const TCHAR* GetCommandLineArgument() override { return TEXT("audiotrace"); }
		//~ End TraceServices::IModule interface

		template <typename TraceProviderType>
		TSharedPtr<TraceProviderType> FindAudioTraceProvider() const
		{
			return StaticCastSharedPtr<TraceProviderType>(TraceProviders.FindRef(TraceProviderType::GetName_Static()));
		}

		virtual void AddTraceProvider(TSharedPtr<FTraceProviderBase> TraceProvider) override;

		virtual void StartTraceAnalysis(const bool bOnlyTraceAudioChannels) override;
		virtual bool IsTraceAnalysisActive() const override;
		virtual void StopTraceAnalysis() override;
		virtual void OnOnlyTraceAudioChannelsStateChanged(const bool bOnlyTraceAudioChannels) override;

		virtual bool AudioChannelsCanBeManuallyEnabled() const override;

#if !WITH_EDITOR
		virtual void InitializeSessionInfo(const TraceServices::FSessionInfo& SessionInfo) override;
		virtual void RequestChannelUpdate() override;
		virtual void ResetTicker() override;
		virtual bool TraceControllerIsAvailable() const override;
#endif

		DECLARE_MULTICAST_DELEGATE_OneParam(FOnAnalysisStarting, const double /*Timestamp*/);
		FOnAnalysisStarting OnAnalysisStarting;

	private:
		static const FName GetName();

		static void DisableAllTraceChannels();
		bool EnableAudioInsightsTraceChannels();

		void DisableAudioInsightsTraceChannels() const;

		void CacheCurrentlyEnabledTraceChannels();
		void RestoreCachedChannels() const;

#if !WITH_EDITOR
		const UE::Trace::FStoreClient::FSessionInfo* GetCurrentAnalysisSessionInfo() const;
		bool GetAudioTracesAreEnabled() const;
		void SendDiscoveryRequestToTraceController() const;

		bool Tick(float DeltaTime);
#endif

		TMap<FName, TSharedPtr<FTraceProviderBase>> TraceProviders;
		TArray<FString> ChannelsToRestore;
		FRewindDebugger RewindDebugger;

		bool bTraceAnalysisHasStarted = false;
		bool bStopTracingAfterAudioInsightsIsFinished = false;

#if !WITH_EDITOR
		FGuid InstanceID;

		FTickerDelegate OnTick;
		FTSTicker::FDelegateHandle OnTickHandle;

		const TArray<FString> AudioChannels;
		const TArray<FString> EmptyArray;
#endif
	};
} // namespace UE::Audio::Insights
