// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDefines.h"
#include "Messages/AnalyzerMessageQueue.h"
#include "Trace/Analyzer.h"
#include "Views/TableDashboardViewFactory.h"

class UAudioBus;

namespace UE::Audio::Insights
{
	enum class EAudioBusEntryType : uint8
	{
		AssetBased,
		CodeGenerated,
		None
	};

	struct FAudioBusMessageBase
	{
		FAudioBusMessageBase() = default;

		FAudioBusMessageBase(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
		{
			const UE::Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

			DeviceId = static_cast<::Audio::FDeviceId>(EventData.GetValue<uint32>("DeviceId"));
			AudioBusId = EventData.GetValue<uint32>("AudioBusId");
			Timestamp = InContext.EventTime.AsSeconds(EventData.GetValue<uint64>("Timestamp"));
		}

		::Audio::FDeviceId DeviceId = INDEX_NONE;
		uint32 AudioBusId = INDEX_NONE;
		double Timestamp = 0.0;
	};

	struct FAudioBusActivateMessage : public FAudioBusMessageBase
	{
		FAudioBusActivateMessage() = default;

		FAudioBusActivateMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FAudioBusMessageBase(InContext)
		{
			const UE::Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

			EventData.GetString("Name", Name);
		}

		FString Name;
	};

	using FAudioBusDeactivateMessage = FAudioBusMessageBase;

	struct FAudioBusHasActivityMessage : public FAudioBusMessageBase
	{
		FAudioBusHasActivityMessage() = default;

		FAudioBusHasActivityMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FAudioBusMessageBase(InContext)
		{
			const UE::Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

			bHasActivity = EventData.GetValue<bool>("HasActivity");
		}

		bool bHasActivity = false;
	};

	class FAudioBusMessages
	{
		UE::Audio::Insights::TAnalyzerMessageQueue<FAudioBusActivateMessage> ActivateMessages{ 0.1 };
		UE::Audio::Insights::TAnalyzerMessageQueue<FAudioBusDeactivateMessage> DeactivateMessages{ 0.1 };
		UE::Audio::Insights::TAnalyzerMessageQueue<FAudioBusHasActivityMessage> HasActivityMessages{ 0.1 };

		friend class FAudioBusProvider;
	};

	struct FAudioBusAssetDashboardEntry : public UE::Audio::Insights::FSoundAssetDashboardEntry
	{
		FAudioBusAssetDashboardEntry() = default;
		virtual ~FAudioBusAssetDashboardEntry() = default;

		EAudioBusEntryType EntryType = EAudioBusEntryType::None;

		TWeakObjectPtr<UAudioBus> AudioBus;
		uint32 AudioBusId = INDEX_NONE;
		bool bHasActivity = false;
	};
} // namespace UE::Audio::Insights
