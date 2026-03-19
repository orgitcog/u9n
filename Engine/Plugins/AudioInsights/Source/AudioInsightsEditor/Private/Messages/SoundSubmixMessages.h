// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDefines.h"
#include "Math/NumericLimits.h"
#include "Messages/AnalyzerMessageQueue.h"
#include "Trace/Analyzer.h"
#include "Views/TableDashboardViewFactory.h"

class USoundSubmix;

namespace UE::Audio::Insights
{
	struct FSoundSubmixMessageBase
	{
		FSoundSubmixMessageBase() = default;

		FSoundSubmixMessageBase(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
		{
			const UE::Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

			DeviceId = static_cast<::Audio::FDeviceId>(EventData.GetValue<uint32>("DeviceId"));
			SubmixId = EventData.GetValue<uint32>("SubmixId");
			Timestamp = InContext.EventTime.AsSeconds(EventData.GetValue<uint64>("Timestamp"));
		}

		::Audio::FDeviceId DeviceId = INDEX_NONE;
		uint32 SubmixId = INDEX_NONE;
		double Timestamp = 0.0;
	};

	struct FSoundSubmixHasActivityMessage : public FSoundSubmixMessageBase
	{
		FSoundSubmixHasActivityMessage() = default;

		FSoundSubmixHasActivityMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FSoundSubmixMessageBase(InContext)
		{
			const UE::Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

			bHasActivity = EventData.GetValue<bool>("HasActivity");
		}

		bool bHasActivity = false;
	};

	class FSoundSubmixMessages
	{
		UE::Audio::Insights::TAnalyzerMessageQueue<FSoundSubmixHasActivityMessage> HasActivityMessages{ 0.1 };

		friend class FSoundSubmixProvider;
	};

	struct FSoundSubmixAssetDashboardEntry : public UE::Audio::Insights::FSoundAssetDashboardEntry
	{
		FSoundSubmixAssetDashboardEntry() = default;
		virtual ~FSoundSubmixAssetDashboardEntry() = default;

		uint32 SubmixId = INDEX_NONE;
		bool bHasActivity = false;
	};
} // namespace UE::Audio::Insights
