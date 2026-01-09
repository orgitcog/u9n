// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioInsightsTraceProviderBase.h"
#include "Messages/MixerSourceTraceMessages.h"
#include "UObject/NameTypes.h"


namespace UE::Audio::Insights
{
	class FMixerSourceTraceProvider
		: public TDeviceDataMapTraceProvider<uint32, TSharedPtr<FMixerSourceDashboardEntry>>
		, public TSharedFromThis<FMixerSourceTraceProvider>
	{
	public:
		FMixerSourceTraceProvider()
			: TDeviceDataMapTraceProvider<uint32, TSharedPtr<FMixerSourceDashboardEntry>>(GetName_Static())
		{
			
		}

		virtual ~FMixerSourceTraceProvider() = default;
		virtual UE::Trace::IAnalyzer* ConstructAnalyzer(TraceServices::IAnalysisSession& InSession) override;

		virtual bool ProcessMessages() override;

		static FName GetName_Static();

#if !WITH_EDITOR
		virtual void InitSessionCachedMessages(TraceServices::IAnalysisSession& InSession) override;
#endif // !WITH_EDITOR

	private:
#if !WITH_EDITOR
		virtual void OnTimingViewTimeMarkerChanged(double TimeMarker) override;

		TUniquePtr<FMixerSourceSessionCachedMessages> SessionCachedMessages;
#endif // !WITH_EDITOR

		FMixerSourceMessages TraceMessages;
	};
} // namespace UE::Audio::Insights
