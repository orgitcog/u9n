// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDefines.h"
#include "AudioInsightsDataSource.h"
#include "Templates/SharedPointer.h"
#include "Views/DashboardViewFactory.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

class SHeaderRow;
class SSearchBox;
class STableViewBase;

namespace UE::Audio::Insights
{
	/**
	 * Tree view entries can inherit from this class to implement extra UObject functionality (ex: open, browse, edit, etc.) 
	 */
	class AUDIOINSIGHTS_API IObjectTreeDashboardEntry : public IDashboardDataTreeViewEntry
	{
	public:
		virtual ~IObjectTreeDashboardEntry() = default;

		virtual TObjectPtr<UObject> GetObject() = 0;
		virtual const TObjectPtr<UObject> GetObject() const = 0;

		virtual FText GetDisplayName() const = 0;
	};

	/** 
	 * Inherit from this class to create a tree view dashboard for Audio Insights.
	 * It contains a search textbox, filters can be optionally be implemented via GetFilterBarWidget and GetFilterBarButtonWidget.
	 * Item actions can be done via OnSelectionChanged, OnDataRowKeyInput, OnConstructContextMenu (for right click)
	 */
	class AUDIOINSIGHTS_API FTraceTreeDashboardViewFactory : public FTraceDashboardViewFactoryBase, public TSharedFromThis<FTraceTreeDashboardViewFactory>
	{
	public:
		FTraceTreeDashboardViewFactory();
		virtual ~FTraceTreeDashboardViewFactory();

		enum class EProcessReason : uint8
		{
			None,
			FilterUpdated,
			EntriesUpdated
		};

	protected:
		struct SRowWidget : public SMultiColumnTableRow<TSharedPtr<IDashboardDataTreeViewEntry>>
		{
			SLATE_BEGIN_ARGS(SRowWidget){}
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<IDashboardDataTreeViewEntry> InData, TSharedRef<FTraceTreeDashboardViewFactory> InFactory);
			virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override;

			TSharedPtr<IDashboardDataTreeViewEntry> Data;
			TSharedPtr<FTraceTreeDashboardViewFactory> Factory;
		};

		struct FHeaderRowColumnData
		{
			const FText DisplayName;
			const FName IconName;
			const bool bShowDisplayName = true;
			const bool bDefaultHidden = false;
			const float FillWidth = 1.0f;
			const EHorizontalAlignment Alignment = HAlign_Left;
		};

		struct FColumnData
		{
			const TFunction<FText(const IDashboardDataTreeViewEntry&)> GetDisplayValue;
			const TFunction<FName(const IDashboardDataTreeViewEntry&)> GetIconName;
			const TFunction<FSlateColor(const IDashboardDataTreeViewEntry&)> GetTextColorValue;
		};

		virtual TSharedPtr<SWidget> GetFilterBarWidget();
		virtual TSharedPtr<SWidget> GetFilterBarButtonWidget();

		virtual TSharedRef<SWidget> MakeWidget(TSharedRef<SDockTab> OwnerTab, const FSpawnTabArgs& SpawnTabArgs);

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const TSharedRef<FTraceTreeDashboardViewFactory::SRowWidget>& InRowWidget, const TSharedRef<IDashboardDataTreeViewEntry>& InRowData, const FName& Column) = 0;
		
		virtual TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<IDashboardDataTreeViewEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);

		virtual TSharedPtr<SWidget> OnConstructContextMenu();

		virtual void OnSelectionChanged(TSharedPtr<IDashboardDataTreeViewEntry> SelectedItem, ESelectInfo::Type SelectInfo);
		virtual FReply OnDataRowKeyInput(const FGeometry& Geometry, const FKeyEvent& KeyEvent) const;

		const FText& GetSearchFilterText() const;

		virtual void RefreshFilteredEntriesListView();

		virtual const TMap<FName, FHeaderRowColumnData>& GetHeaderRowColumns() const = 0;
		virtual const TMap<FName, FColumnData>& GetColumns() const = 0;

		virtual void ProcessEntries(EProcessReason Reason) = 0;
		virtual void SortTable() = 0;

		virtual TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& GetTreeItemsSource() { return DataViewEntries; }

		virtual bool ResetTreeData()
		{
			if (!DataViewEntries.IsEmpty())
			{
				DataViewEntries.Empty();
				return true;
			}

			return false;
		}

		void Tick(float InElapsed);

#if WITH_EDITOR
		virtual bool IsDebugDrawEnabled() const { return false; }
		virtual void DebugDraw(float InElapsed, const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& InSelectedItems, ::Audio::FDeviceId InAudioDeviceId) const {};
#endif // WITH_EDITOR

		template<typename TableProviderType>
		bool FilterEntries(TFunctionRef<bool(IDashboardDataTreeViewEntry&)> InPredicate)
		{
			const TSharedPtr<const TableProviderType> Provider = FindProvider<const TableProviderType>();
			if (Provider.IsValid())
			{
				if (const typename TableProviderType::FDeviceData* DeviceData = Provider->FindFilteredDeviceData())
				{
					DataViewEntries.Reset();

					// Filter Entries
					const auto TransformEntry = [](const typename TableProviderType::FEntryPair& Pair)
					{
						return StaticCastSharedPtr<IDashboardDataTreeViewEntry>(Pair.Value);
					};

					const auto FilterEntry = [this, &InPredicate](const typename TableProviderType::FEntryPair& Pair)
					{
						return InPredicate(*Pair.Value);
					};

					Algo::TransformIf(*DeviceData, DataViewEntries, FilterEntry, TransformEntry);

					// Sort list
					RequestSort();

					return true;
				}
				else
				{
					return ResetTreeData();
				}
			}

			return false;
		}

		EProcessReason UpdateFilterReason = EProcessReason::None;
		FTSTicker::FDelegateHandle TickerHandle;

		TArray<TSharedPtr<IDashboardDataTreeViewEntry>> DataViewEntries;
		TMap<FName, uint64> UpdateIds;

		TSharedPtr<SWidget> DashboardWidget;
		TSharedPtr<SHeaderRow> HeaderRowWidget;
		TSharedPtr<STreeView<TSharedPtr<IDashboardDataTreeViewEntry>>> FilteredEntriesListView;

		FName SortByColumn;
		EColumnSortMode::Type SortMode = EColumnSortMode::None;

	private:
		TSharedRef<SHeaderRow> MakeHeaderRowWidget();
		void SetSearchBoxFilterText(const FText& NewText);

		EColumnSortMode::Type GetColumnSortMode(const FName InColumnId) const;
		void RequestSort();
		void OnColumnSortModeChanged(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId, const EColumnSortMode::Type InSortMode);
		void HandleRecursiveExpansion(TSharedPtr<IDashboardDataTreeViewEntry> Item, bool bIsItemExpanded);

		TSharedPtr<SSearchBox> SearchBoxWidget;
		FText SearchBoxFilterText;
	};
} // namespace UE::Audio::Insights
