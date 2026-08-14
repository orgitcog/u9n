// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"
#include "Views/TableDashboardViewFactory.h"

class UAudioBus;

namespace UE::Audio::Insights
{
	class FAudioBusProvider;

	class FAudioBusesDashboardViewFactory : public FTraceObjectTableDashboardViewFactory
	{
	public:
		FAudioBusesDashboardViewFactory();
		virtual ~FAudioBusesDashboardViewFactory();

		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual EDefaultDashboardTabStack GetDefaultTabStack() const override;
		virtual TSharedRef<SWidget> MakeWidget(TSharedRef<SDockTab> OwnerTab, const FSpawnTabArgs& SpawnTabArgs) override;

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBusAssetInit, const bool /*bIsChecked*/, TWeakObjectPtr<UAudioBus> /*AudioBus*/);
		inline static FOnBusAssetInit OnBusAssetInit;

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAudioBusAssetChecked, const bool /*bIsChecked*/, const TWeakObjectPtr<UAudioBus> /*AudioBus*/);
		inline static FOnAudioBusAssetChecked OnAudioBusAssetChecked;

	private:
		enum class EAudioBusTypeComboboxSelection : uint8
		{
			AssetBased,
			CodeGenerated,
			All
		};

		TSharedRef<SWidget> MakeAudioBusTypeFilterWidget();

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& InColumnName) override;
		virtual void ProcessEntries(FTraceTableDashboardViewFactory::EProcessReason Reason) override;
		virtual const TMap<FName, FTraceTableDashboardViewFactory::FColumnData>& GetColumns() const override;

		virtual void SortTable() override;

		void FilterByAudioBusName();
		void FilterByAudioBusType();

		void RequestListRefresh();

		void HandleOnAudioBusAssetListUpdated(const TWeakObjectPtr<UObject> InAsset);

		TSharedPtr<FAudioBusProvider> AudioBusProvider;
		TMap<const TWeakObjectPtr<UAudioBus>, bool> AudioBusCheckboxCheckedStates;

		using FComboboxSelectionItem = TPair<EAudioBusTypeComboboxSelection, FText>;
		TArray<TSharedPtr<FComboboxSelectionItem>> AudioBusTypes;
		TSharedPtr<FComboboxSelectionItem> SelectedAudioBusType;
	};
} // namespace UE::Audio::Insights
