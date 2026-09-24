// Copyright Epic Games, Inc. All Rights Reserved.
#include "Messages/SoundTraceMessages.h"

namespace UE::Audio::Insights
{
	namespace SoundClassNames
	{
		const FString MetaSoundSource = TEXT("MetaSoundSource");
		const FString SoundWaveProcedural = TEXT("SoundWaveProcedural");
		const FString SoundCue = TEXT("SoundCue");
		const FString SoundWave = TEXT("SoundWave");
		const FString SoundCueTemplate = TEXT("SoundCueTemplate");
	}

	FSoundMessageBase::FSoundMessageBase(const Trace::IAnalyzer::FOnEventContext& InContext)
	{
		const Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

		DeviceId = static_cast<::Audio::FDeviceId>(EventData.GetValue<uint32>("DeviceId"));
		Timestamp = InContext.EventTime.AsSeconds(EventData.GetValue<uint64>("Timestamp"));
		PlayOrder = EventData.GetValue<uint32>("PlayOrder");
	}

	FSoundStartMessage::FSoundStartMessage(const Trace::IAnalyzer::FOnEventContext& InContext)
		: FSoundMessageBase(InContext)
	{
		const Trace::IAnalyzer::FEventData& EventData = InContext.EventData;
		EventData.GetString("Name", Name);

		FString SoundClassName;
		FString SoundSuperClassName;

		EventData.GetString("SoundClassName", SoundClassName);
		EventData.GetString("SoundSuperClassName", SoundSuperClassName);

		if (SoundClassName == SoundClassNames::SoundCue)
		{
			EntryType = ESoundDashboardEntryType::SoundCue;
		}
		else if (SoundClassName == SoundClassNames::SoundWave)
		{
			EntryType = ESoundDashboardEntryType::SoundWave;
		}
		else if (SoundClassName == SoundClassNames::MetaSoundSource)
		{
			EntryType = ESoundDashboardEntryType::MetaSound;
		}
		else if (SoundClassName == SoundClassNames::SoundWaveProcedural
			|| SoundSuperClassName == SoundClassNames::SoundWaveProcedural)
		{
			EntryType = ESoundDashboardEntryType::ProceduralSource;
		}
		else if (SoundClassName == SoundClassNames::SoundCueTemplate
			|| SoundSuperClassName == SoundClassNames::SoundCueTemplate)
		{
			EntryType = ESoundDashboardEntryType::SoundCueTemplate;
		}
		else
		{
			EntryType = ESoundDashboardEntryType::None;
		}
	}

	FSoundDashboardEntry::FSoundDashboardEntry()
	{
		constexpr uint32 DataPointsCapacity = 256;

		PriorityDataPoints.SetCapacity(DataPointsCapacity);
		DistanceDataPoints.SetCapacity(DataPointsCapacity);
		AmplitudeDataPoints.SetCapacity(DataPointsCapacity);
		VolumeDataPoints.SetCapacity(DataPointsCapacity);
		PitchDataPoints.SetCapacity(DataPointsCapacity);
	}
}