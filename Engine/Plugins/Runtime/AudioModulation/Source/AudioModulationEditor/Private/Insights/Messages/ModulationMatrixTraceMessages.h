// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDefines.h"
#include "Messages/AnalyzerMessageQueue.h"
#include "Trace/Analyzer.h"
#include "Views/TableDashboardViewFactory.h"

namespace AudioModulationEditor
{
	using FBusId    = uint32;
	using FSourceId = uint32;

	enum class EModulationMatrixEntryType : uint8
	{
		// Arranged alphabetically to simplify sorting
		BusMix,
		BusFinalValues,
		Generator,
		None
	};


	// Modulation matrix dashboard entry
	class FModulationMatrixDashboardEntry : public UE::Audio::Insights::FSoundAssetDashboardEntry
	{
	public:
		FModulationMatrixDashboardEntry() = default;
		virtual ~FModulationMatrixDashboardEntry() = default;

		FSourceId SourceId = INDEX_NONE;
		EModulationMatrixEntryType EntryType = EModulationMatrixEntryType::None;

		TMap<FBusId, float> BusIdToValueMap;
	};
	

	// Modulation matrix message base
	struct FModulationMatrixMessageBase
	{
		FModulationMatrixMessageBase() = default;

		FModulationMatrixMessageBase(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
		{
			const UE::Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

			DeviceId  = static_cast<::Audio::FDeviceId>(EventData.GetValue<uint32>("DeviceId"));
			Timestamp = InContext.EventTime.AsSeconds(EventData.GetValue<uint64>("Timestamp"));
			SourceId  = static_cast<FSourceId>(EventData.GetValue<uint32>("SourceId"));
		}

		Audio::FDeviceId DeviceId    = INDEX_NONE;
		FSourceId SourceId = INDEX_NONE;
		double Timestamp = 0.0;
	};


	// Register bus message
	struct FModulationMatrixRegisterBusMessage : public FModulationMatrixMessageBase
	{
		FModulationMatrixRegisterBusMessage() = default;

		FModulationMatrixRegisterBusMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FModulationMatrixMessageBase(InContext)
		{
			const UE::Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

			ModulatingSourceId = static_cast<FSourceId>(EventData.GetValue<uint32>("ModulatingSourceId"));
			EventData.GetString("BusName", BusName);
		}

		FSourceId ModulatingSourceId = INDEX_NONE;
		FString BusName;
	};


	// Activate messages
	struct FModulationMatrixActivateMessage : public FModulationMatrixMessageBase
	{
		FModulationMatrixActivateMessage() = default;

		FModulationMatrixActivateMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FModulationMatrixMessageBase(InContext)
		{
			const UE::Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

			EventData.GetString("Name", Name);
		}

		EModulationMatrixEntryType EntryType = EModulationMatrixEntryType::None;
		FString Name;
	};

	struct FBusMixActivateMessage : public FModulationMatrixActivateMessage
	{
		FBusMixActivateMessage() = default;

		FBusMixActivateMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FModulationMatrixActivateMessage(InContext)
		{
			EntryType = EModulationMatrixEntryType::BusMix;
		}
	};

	struct FGeneratorActivateMessage : public FModulationMatrixActivateMessage
	{
		FGeneratorActivateMessage() = default;

		FGeneratorActivateMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FModulationMatrixActivateMessage(InContext)
		{
			EntryType = EModulationMatrixEntryType::Generator;
		}
	};


	// Update messages
	struct FModulationMatrixUpdateMessage : public FModulationMatrixMessageBase
	{
		FModulationMatrixUpdateMessage() = default;

		FModulationMatrixUpdateMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FModulationMatrixMessageBase(InContext)
		{
			const UE::Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

			const TArrayView<const FBusId> BusIds   = EventData.GetArrayView<FBusId>("BusIds");
			const TArrayView<const float> BusValues = EventData.GetArrayView<float>("BusValues");

			check(BusIds.Num() == BusValues.Num());

			for (int32 Index = 0; Index < BusIds.Num(); ++Index)
			{
				BusIdToValueMap.Emplace(BusIds[Index], BusValues[Index]);
			}
		}

		TMap<FBusId, float> BusIdToValueMap;
	};

	using FBusMixUpdateMessage = FModulationMatrixUpdateMessage;

	struct FGeneratorUpdateMessage : public FModulationMatrixMessageBase
	{
		FGeneratorUpdateMessage() = default;

		FGeneratorUpdateMessage(const UE::Trace::IAnalyzer::FOnEventContext& InContext)
			: FModulationMatrixMessageBase(InContext)
		{
			const UE::Trace::IAnalyzer::FEventData& EventData = InContext.EventData;

			const TArrayView<const FBusId> BusIds = EventData.GetArrayView<FBusId>("BusIds");
			const float GeneratorValue = EventData.GetValue<float>("GeneratorValue");

			// All control buses use the same generator value
			for (int32 Index = 0; Index < BusIds.Num(); ++Index)
			{
				BusIdToValueMap.Emplace(BusIds[Index], GeneratorValue);
			}
		}

		TMap<FBusId, float> BusIdToValueMap;
	};

	using FBusFinalValuesUpdateMessage = FModulationMatrixUpdateMessage;


	// Deactivate message
	using FModulationMatrixDeactivateMessage = FModulationMatrixMessageBase;


	// Modulation matrix messages
	class FModulationMatrixMessages
	{
		UE::Audio::Insights::TAnalyzerMessageQueue<FModulationMatrixRegisterBusMessage> RegisterBusMessages{ 0.1 };

		UE::Audio::Insights::TAnalyzerMessageQueue<FBusMixActivateMessage> BusMixActivateMessages{ 0.1 };
		UE::Audio::Insights::TAnalyzerMessageQueue<FBusMixUpdateMessage> BusMixUpdateMessages{ 0.1 };

		UE::Audio::Insights::TAnalyzerMessageQueue<FGeneratorActivateMessage> GeneratorActivateMessages{ 0.1 };
		UE::Audio::Insights::TAnalyzerMessageQueue<FGeneratorUpdateMessage> GeneratorUpdateMessages{ 0.1 };

		UE::Audio::Insights::TAnalyzerMessageQueue<FBusFinalValuesUpdateMessage> BusFinalValuesUpdateMessages{ 0.1 };

		UE::Audio::Insights::TAnalyzerMessageQueue<FModulationMatrixDeactivateMessage> DeactivateMessages{ 0.1 };

		friend class FModulationMatrixTraceProvider;
	};

} // namespace AudioModulationEditor
