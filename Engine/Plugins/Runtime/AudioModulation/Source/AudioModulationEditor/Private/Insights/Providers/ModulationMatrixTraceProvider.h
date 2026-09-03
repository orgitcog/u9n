// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioInsightsTraceProviderBase.h"
#include "Containers/Map.h"
#include "Delegates/DelegateCombinations.h"
#include "Insights/Messages/ModulationMatrixTraceMessages.h"

namespace AudioModulationEditor
{
	class FModulationMatrixTraceProvider
		: public UE::Audio::Insights::TDeviceDataMapTraceProvider<uint32, TSharedPtr<FModulationMatrixDashboardEntry>>
		, public TSharedFromThis<FModulationMatrixTraceProvider>
	{
	public:
		FModulationMatrixTraceProvider();

		virtual ~FModulationMatrixTraceProvider();

		static FName GetName_Static();

		virtual bool ProcessMessages() override;
		virtual UE::Trace::IAnalyzer* ConstructAnalyzer(TraceServices::IAnalysisSession& InSession) override;

		struct BusInfo
		{
			FString BusName;
			int32 RefCount = 0;
		};

		using BusIdToBusInfoMap = TMap<FBusId, BusInfo>;

		DECLARE_DELEGATE_OneParam(FOnControlBusesAdded, const BusIdToBusInfoMap& /*AddedControlBuses*/);
		FOnControlBusesAdded OnControlBusesAdded;

		DECLARE_DELEGATE_OneParam(FOnControlBusesRemoved, const TArray<FName>& /*RemovedControlBusesNames*/);
		FOnControlBusesRemoved OnControlBusesRemoved;

	private:
		void UpdateActiveControlBusesToAdd(const TMap<FBusId, FString>& InBusIdToBusNameMap);
		void UpdateActiveControlBusesToRemove(const TMap<FBusId, float>& InBusIdToValueMap);
		void OnAudioDeviceDestroyed(::Audio::FDeviceId InDeviceId);

		FModulationMatrixMessages TraceMessages;

		TMap<::Audio::FDeviceId, TSet<uint32>> DeviceIdToActiveModulatorSourceIdsMap;
		BusIdToBusInfoMap ActiveControlBuses;
		TArray<FName> RemovedControlBusesNames;
	};
} // namespace AudioModulationEditor
