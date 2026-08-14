// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDefines.h"
#include "DSP/Dsp.h"
#include "Messages/AnalyzerMessageQueue.h"
#include "Messages/MixerSourceTraceMessages.h"
#include "Trace/Analyzer.h"
#include "UObject/SoftObjectPath.h"
#include "Views/TreeDashboardViewFactory.h"

#if !WITH_EDITOR
#include "Common/PagedArray.h"
#include "TraceServices/Model/AnalysisSession.h"
#endif // !WITH_EDITOR

namespace UE::Audio::Insights
{
	static constexpr double INVALID_TIMEOUT = -1.0;

	enum class ESoundDashboardEntryType : uint8
	{
		None,
		MetaSound,
		SoundCue,
		ProceduralSource,
		SoundWave,
		SoundCueTemplate,
		Pinned
	};

	// Trace messages
	struct FSoundMessageBase
	{
		FSoundMessageBase() = default;
		FSoundMessageBase(const Trace::IAnalyzer::FOnEventContext& InContext);

		::Audio::FDeviceId DeviceId = INDEX_NONE;
		uint32 PlayOrder = INDEX_NONE;
		double Timestamp = 0.0;
	};

	struct FSoundStartMessage : public FSoundMessageBase
	{
		FSoundStartMessage() = default;
		FSoundStartMessage(const Trace::IAnalyzer::FOnEventContext& InContext);

		FString Name;
		ESoundDashboardEntryType EntryType;
	};

	struct FSoundWaveStartMessage : public FSoundStartMessage
	{
		FSoundWaveStartMessage() = default;

		FSoundWaveStartMessage(const Trace::IAnalyzer::FOnEventContext& InContext)
			: FSoundStartMessage(InContext)
		{
			ActiveSoundPlayOrder = InContext.EventData.GetValue<uint32>("ActiveSoundPlayOrder");
		}

		uint32 ActiveSoundPlayOrder;
	};

	using FSoundStopMessage = FSoundMessageBase;

	struct FSoundPriorityMessage : public FMixerSourceParameterMessage
	{
		FSoundPriorityMessage() = default;
		FSoundPriorityMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FMixerSourceParameterMessage(InContext)
		{
			Priority = InContext.EventData.GetValue<float>("Priority");
		}

		float Priority = 0.0f;
	};

	struct FSoundDistanceMessage : public FMixerSourceParameterMessage
	{
		FSoundDistanceMessage() = default;
		FSoundDistanceMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FMixerSourceParameterMessage(InContext)
		{
			Distance = InContext.EventData.GetValue<float>("Distance");
		}

		float Distance = 0.0f;
	};



	// Trace message queues
	class FSoundMessages
	{
	public:
		TAnalyzerMessageQueue<FSoundStartMessage> ActiveSoundStartMessages { 0.1 };
		TAnalyzerMessageQueue<FSoundWaveStartMessage> SoundWaveStartMessages{ 0.1 };
		TAnalyzerMessageQueue<FSoundPriorityMessage> PriorityMessages{ 0.1 };
		TAnalyzerMessageQueue<FSoundDistanceMessage> DistanceMessages { 0.1 };
		TAnalyzerMessageQueue<FMixerSourceEnvelopeMessage> AmplitudeMessages { 0.1 };
		TAnalyzerMessageQueue<FMixerSourceVolumeMessage> VolumeMessages { 0.1 };
		TAnalyzerMessageQueue<FMixerSourcePitchMessage> PitchMessages { 0.1 };
		TAnalyzerMessageQueue<FSoundStopMessage> StopMessages { 0.1 };
	};

#if !WITH_EDITOR
	struct FSoundSessionCachedMessages
	{
		FSoundSessionCachedMessages(TraceServices::IAnalysisSession& InSession)
			: StartCachedMessages(InSession.GetLinearAllocator(), 16384)
			, SoundWaveStartCachedMessages(InSession.GetLinearAllocator(), 16384)
			, PriorityCachedMessages(InSession.GetLinearAllocator(), 16384)
			, DistanceCachedMessages(InSession.GetLinearAllocator(), 16384)
			, AmplitudeCachedMessages(InSession.GetLinearAllocator(), 16384)
			, VolumeCachedMessages(InSession.GetLinearAllocator(), 16384)
			, PitchCachedMessages(InSession.GetLinearAllocator(), 16384)
			, StopCachedMessages(InSession.GetLinearAllocator(), 4096)
		{

		}

		TraceServices::TPagedArray<FSoundStartMessage> StartCachedMessages;
		TraceServices::TPagedArray<FSoundWaveStartMessage> SoundWaveStartCachedMessages;
		TraceServices::TPagedArray<FSoundPriorityMessage> PriorityCachedMessages;
		TraceServices::TPagedArray<FSoundDistanceMessage> DistanceCachedMessages;
		TraceServices::TPagedArray<FMixerSourceEnvelopeMessage> AmplitudeCachedMessages;
		TraceServices::TPagedArray<FMixerSourceVolumeMessage> VolumeCachedMessages;
		TraceServices::TPagedArray<FMixerSourcePitchMessage> PitchCachedMessages;
		TraceServices::TPagedArray<FSoundStopMessage> StopCachedMessages;
	};
#endif // !WITH_EDITOR


	// Dashboard entry
	using FDataPoint = TPair<double, float>; // (Timestamp, Value)

	class FSoundDashboardEntry : public IObjectTreeDashboardEntry
	{
	public:
		FSoundDashboardEntry();
		virtual ~FSoundDashboardEntry() = default;

		virtual TObjectPtr<UObject> GetObject() override { return FSoftObjectPath(Name).ResolveObject(); }
		virtual const TObjectPtr<UObject> GetObject() const override { return FSoftObjectPath(Name).ResolveObject(); }

		virtual FText GetDisplayName() const override
		{
			const FSoftObjectPath AssetPath(Name);
			return FText::FromString(AssetPath.IsValid() ? AssetPath.GetAssetName() : Name);
		}

		virtual bool IsValid() const override { return PlayOrder != INDEX_NONE; }

		virtual bool ShouldInitExpandChildren() const override { return bShouldForceExpandChildren; }
		virtual void ResetShouldInitExpandChildren() override { bShouldForceExpandChildren = false; }

		enum class EPinnedEntryType : uint8
		{
			None = 0,
			PinnedCopy,
			HiddenOriginalEntry
		};

		::Audio::FDeviceId DeviceId = INDEX_NONE;
		uint32 PlayOrder = INDEX_NONE;
		double Timestamp = 0.0;
		double TimeoutTimestamp = INVALID_TIMEOUT;
		FString Name;
		ESoundDashboardEntryType EntryType = ESoundDashboardEntryType::None;
		EPinnedEntryType PinnedEntryType = EPinnedEntryType::None;
		bool bIsCategory = false;
		bool bShouldForceExpandChildren = true;
		bool bIsVisible = false;

		::Audio::TCircularAudioBuffer<FDataPoint> PriorityDataPoints;
		::Audio::TCircularAudioBuffer<FDataPoint> DistanceDataPoints;
		::Audio::TCircularAudioBuffer<FDataPoint> AmplitudeDataPoints;
		::Audio::TCircularAudioBuffer<FDataPoint> VolumeDataPoints;
		::Audio::TCircularAudioBuffer<FDataPoint> PitchDataPoints;
	};
} // namespace UE::Audio::Insights
