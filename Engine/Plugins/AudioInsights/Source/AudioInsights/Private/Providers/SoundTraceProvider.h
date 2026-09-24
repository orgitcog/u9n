// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioInsightsTraceProviderBase.h"
#include "Messages/SoundTraceMessages.h"
#include "UObject/NameTypes.h"

/**
 *	Trace provider for the "Sounds Dashboard"
 *	
 *	"Sounds Dashboard" displays active audio in the following tree structure:
 *	- Category											(i.e. The EntryType of the sound asset, e.g. SoundCue, MetaSound etc)
 *		- Active Sound									(i.e. The Active Sound instance playing)
 *			- SoundWave Instances (optional)			(i.e. If an entry type can have multiple SoundWaves playing simultaneously (e.g. SoundCue)
 *														, individual Soundwaves are shown here)
 */
namespace UE::Audio::Insights
{
	class FSoundTraceProvider
		: public TDeviceDataMapTraceProvider<ESoundDashboardEntryType, TSharedPtr<FSoundDashboardEntry>>
		, public TSharedFromThis<FSoundTraceProvider>
	{
	public:
		FSoundTraceProvider();
		virtual ~FSoundTraceProvider() = default;

		static FName GetName_Static();

		virtual UE::Trace::IAnalyzer* ConstructAnalyzer(TraceServices::IAnalysisSession& InSession) override;

#if !WITH_EDITOR
		virtual void InitSessionCachedMessages(TraceServices::IAnalysisSession& InSession) override;
#endif // !WITH_EDITOR

		virtual bool ProcessMessages() override;
		virtual bool ProcessManuallyUpdatedEntries() override;

	private:
		void GetOrCreateActiveSoundEntry(const FSoundStartMessage& Msg, TSharedPtr<FSoundDashboardEntry>& OutReturnedSoundEntry);
		void GetActiveSoundEntryFromIDs(const uint32 PlayOrderID, const ::Audio::FDeviceId DeviceId, TSharedPtr<FSoundDashboardEntry>& OutSoundEntry);
		void RemoveActiveSoundEntry(const ::Audio::FDeviceId DeviceId, TSharedPtr<FSoundDashboardEntry> OutEntry);

		void UpdateAggregateActiveSoundData();
		void CollectAggregateData(FSoundDashboardEntry& ActiveSoundEntry);

#if !WITH_EDITOR
		virtual void OnTimingViewTimeMarkerChanged(double TimeMarker) override;

		TUniquePtr<FSoundSessionCachedMessages> SessionCachedMessages;
#endif // !WITH_EDITOR

		FSoundMessages TraceMessages;

		struct SoundEntryKeys
		{
			const ESoundDashboardEntryType EntryType;
		};

		struct SoundMessageIDs
		{
			const ::Audio::FDeviceId DeviceId;
			const uint32 PlayOrder;
		};

		TMap<uint32, SoundEntryKeys> ActiveSoundToEntryKeysMap;
		TArray<SoundMessageIDs> EntriesTimingOut;
		TSet<uint32> SoundsStoppedBeforeStart;
	};
} // namespace UE::Audio::Insights
