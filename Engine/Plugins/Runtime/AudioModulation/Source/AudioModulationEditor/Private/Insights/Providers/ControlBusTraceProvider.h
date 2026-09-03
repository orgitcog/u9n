// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioInsightsTraceProviderBase.h"
#include "Insights/Messages/ControlBusTraceMessages.h"


namespace AudioModulationEditor
{
	class FControlBusTraceProvider
		: public UE::Audio::Insights::TDeviceDataMapTraceProvider<uint32, TSharedPtr<FControlBusDashboardEntry>>
		, public TSharedFromThis<FControlBusTraceProvider>
	{
	public:
		FControlBusTraceProvider()
			: UE::Audio::Insights::TDeviceDataMapTraceProvider<uint32, TSharedPtr<FControlBusDashboardEntry>>(GetName_Static())
		{

		}

		virtual ~FControlBusTraceProvider() = default;
		virtual UE::Trace::IAnalyzer* ConstructAnalyzer(TraceServices::IAnalysisSession& InSession) override;

		virtual bool ProcessMessages() override;
		static FName GetName_Static();

	private:

		FControlBusMessages TraceMessages;
	};
} // namespace AudioModulationEditor
