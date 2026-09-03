// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Insights/Providers/ModulationMatrixTraceProvider.h"
#include "Templates/SharedPointer.h"
#include "Views/TableDashboardViewFactory.h"

namespace UE::Audio::Insights
{
	class IDashboardDataViewEntry;
}

namespace AudioModulationEditor
{
	class FModulationMatrixDashboardViewFactory : public UE::Audio::Insights::FTraceObjectTableDashboardViewFactory
	{
	public:
		FModulationMatrixDashboardViewFactory();
		virtual ~FModulationMatrixDashboardViewFactory() = default;

		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual UE::Audio::Insights::EDefaultDashboardTabStack GetDefaultTabStack() const override;

		virtual TSharedRef<SWidget> MakeWidget(TSharedRef<SDockTab> OwnerTab, const FSpawnTabArgs& SpawnTabArgs) override;

		virtual void ProcessEntries(UE::Audio::Insights::FTraceTableDashboardViewFactory::EProcessReason Reason) override;

	private:
		virtual const TMap<FName, UE::Audio::Insights::FTraceTableDashboardViewFactory::FColumnData>& GetColumns() const override;

		void CreateDefaultColumnData();
		void RegisterDelegates();

		TSharedRef<SWidget> MakeModulatingSourceTypeFilterWidget();

		void OnControlBusesAdded(const FModulationMatrixTraceProvider::BusIdToBusInfoMap& AddedControlBuses);
		void OnControlBusesRemoved(const TArray<FName>& RemovedControlBusesNames);
		void OnAudioDeviceDestroyed(::Audio::FDeviceId InDeviceId);

		void FilterByModulatingSourceName();
		void FilterByModulatingSourceType();

		virtual void SortTable() override;
		virtual FSlateColor GetRowColor(const TSharedPtr<UE::Audio::Insights::IDashboardDataViewEntry>& InRowDataPtr) override;

		enum class EModulatingSourceComboboxSelection : uint8
		{
			All,
			BusMixes,
			Generators
		};

		TSharedPtr<FModulationMatrixTraceProvider> ModulationMatrixTraceProvider;
		TMap<FName, FTraceTableDashboardViewFactory::FColumnData> ModulationMatrixColumnData;
		TArray<FName> ActiveBusNames;

		FDelegateHandle OnControlBusAddedHandle;
		FDelegateHandle OnControlBusRemovedHandle;
		FDelegateHandle OnDeviceDestroyedHandle;

		using FComboboxSelectionItem = TPair<EModulatingSourceComboboxSelection, FText>;
		TArray<TSharedPtr<FComboboxSelectionItem>> ModulatingSourceTypes;
		TSharedPtr<FComboboxSelectionItem> SelectedModulatingSourceType;
	};
} // namespace AudioModulationEditor
