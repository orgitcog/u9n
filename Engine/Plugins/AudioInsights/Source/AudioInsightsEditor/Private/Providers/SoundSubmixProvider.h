// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/Delegate.h"
#include "Messages/SoundSubmixMessages.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "UObject/NameTypes.h"

namespace UE::Audio::Insights
{
	class FSoundSubmixProvider : public TDeviceDataMapTraceProvider<uint32, TSharedPtr<FSoundSubmixAssetDashboardEntry>>, public TSharedFromThis<FSoundSubmixProvider>
	{
	public:
		FSoundSubmixProvider();
		virtual ~FSoundSubmixProvider();

		static FName GetName_Static();

		virtual UE::Trace::IAnalyzer* ConstructAnalyzer(TraceServices::IAnalysisSession& InSession) override;
		
		void RequestEntriesUpdate();

		DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubmixAssetAdded, const uint32 /*SubmixId*/);
		inline static FOnSubmixAssetAdded OnSubmixAssetAdded;

		DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubmixAssetRemoved, const uint32 /*SubmixId*/);
		inline static FOnSubmixAssetRemoved OnSubmixAssetRemoved;

		DECLARE_MULTICAST_DELEGATE(FOnSubmixAssetListUpdated);
		inline static FOnSubmixAssetListUpdated OnSubmixAssetListUpdated;

	private:
		void OnAssetAdded(const FAssetData& InAssetData);
		void OnAssetRemoved(const FAssetData& InAssetData);
		void OnFilesLoaded();
		void OnActiveAudioDeviceChanged();
		void OnTraceStarted(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination);

		void AddSubmixAsset(const FAssetData& InAssetData);
		void RemoveSubmixAsset(const FAssetData& InAssetData);

		void UpdateSubmixAssetNames();

		virtual bool ProcessMessages() override;

		bool bAreFilesLoaded = false;
		bool bAssetEntriesNeedRefreshing  = false;

		TArray<TSharedPtr<FSoundSubmixAssetDashboardEntry>> SubmixDataViewEntries;

		FSoundSubmixMessages TraceMessages;
	};
} // namespace UE::Audio::Insights
