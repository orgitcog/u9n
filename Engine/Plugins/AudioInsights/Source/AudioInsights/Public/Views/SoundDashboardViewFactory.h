// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Views/TreeDashboardViewFactory.h"

#include "Filters/GenericFilter.h"

class FUICommandList;

namespace UE::Audio::Insights
{
	class FSoundDashboardEntry;

	enum class ESoundDashboardFilterFlags : uint32
	{
		None = 0,
		MetaSound = 1 << 0,
		SoundCue  = 1 << 1,
		ProceduralSource = 1 << 2,
		SoundWave = 1 << 3,
		SoundCueTemplate = 1 << 4,
		Pinned = 1 << 5
		// @TODO UE-250399: Hide category pending to implement
		//, Hidden = 1 << 6
	};

	ENUM_CLASS_FLAGS(ESoundDashboardFilterFlags);

#if WITH_EDITOR
	enum class EMuteSoloMode : uint8
	{
		Mute,
		Solo
	};
#endif // WITH_EDITOR

	class FSoundDashboardFilter : public FGenericFilter<ESoundDashboardFilterFlags>
	{
	public:
		FSoundDashboardFilter(ESoundDashboardFilterFlags InFlags, 
			const FString& InName, 
			const FText& InDisplayName, 
			const FName& InIconName,
			const FText& InToolTipText, 
			FLinearColor InColor, 
			TSharedPtr<FFilterCategory> InCategory)
			: FGenericFilter<ESoundDashboardFilterFlags>(InCategory, InName, InDisplayName, FGenericFilter<ESoundDashboardFilterFlags>::FOnItemFiltered())
			, Flags(InFlags)
		{
			ToolTip  = InToolTipText;
			Color    = InColor;
			IconName = InIconName;
		}

		bool IsActive() const {	return bIsActive; }
		ESoundDashboardFilterFlags GetFlags() const { return Flags; }

	private:
		virtual void ActiveStateChanged(bool bActive) override { bIsActive = bActive; }
		virtual bool PassesFilter(ESoundDashboardFilterFlags InItem) const override { return EnumHasAnyFlags(InItem, Flags); }

		ESoundDashboardFilterFlags Flags;
		bool bIsActive = false;
	};

	/**
	* Helper class for pinned items in the dashboard tree
	*	- Contains a weak handle to the original entry (OriginalDataEntry) which is updated from the trace provider
	*	- Copies updated params to PinnedSectionEntry for display
	*/
	class AUDIOINSIGHTS_API FPinnedSoundEntryWrapper
	{
	public:
		FPinnedSoundEntryWrapper() = delete;
		FPinnedSoundEntryWrapper(const TSharedPtr<IDashboardDataTreeViewEntry>& OriginalEntry);

		TSharedPtr<IDashboardDataTreeViewEntry> GetPinnedSectionEntry() const { return PinnedSectionEntry; }
		TSharedPtr<IDashboardDataTreeViewEntry> GetOriginalDataEntry() const { return OriginalDataEntry.IsValid() ? OriginalDataEntry.Pin() : nullptr; }

		TSharedPtr<FPinnedSoundEntryWrapper> AddChildEntry(const TSharedPtr<IDashboardDataTreeViewEntry> Child);

		void UpdateParams();

		void CleanUp();
		void MarkToDelete();

		bool EntryIsValid() const;

		TArray<TSharedPtr<FPinnedSoundEntryWrapper>> PinnedWrapperChildren;

	private:
		TSharedPtr<IDashboardDataTreeViewEntry> PinnedSectionEntry;
		TWeakPtr<IDashboardDataTreeViewEntry> OriginalDataEntry;
	};

	class AUDIOINSIGHTS_API FSoundDashboardViewFactory : public FTraceTreeDashboardViewFactory
	{
	public:
		FSoundDashboardViewFactory();
		virtual ~FSoundDashboardViewFactory();

		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual EDefaultDashboardTabStack GetDefaultTabStack() const override;
		virtual TSharedRef<SWidget> MakeWidget(TSharedRef<SDockTab> OwnerTab, const FSpawnTabArgs& SpawnTabArgs) override;
		virtual void ProcessEntries(FTraceTreeDashboardViewFactory::EProcessReason Reason) override;

	private:
		void BindCommands();

		virtual TSharedPtr<SWidget> GetFilterBarWidget() override;
		virtual TSharedPtr<SWidget> GetFilterBarButtonWidget() override;

		bool IsRootItem(const TSharedRef<IDashboardDataTreeViewEntry>& InEntry) const;
		bool EntryCanHaveChildren(const TSharedRef<IDashboardDataTreeViewEntry>& InEntry) const;
		bool IsDescendant(const TSharedPtr<IDashboardDataTreeViewEntry>& InEntry, const TSharedPtr<IDashboardDataTreeViewEntry>& InChildCandidate) const;

		TSharedRef<SWidget> GenerateWidgetForRootColumn(const TSharedRef<FTraceTreeDashboardViewFactory::SRowWidget>& InRowWidget, const TSharedRef<IDashboardDataTreeViewEntry>& InRowData, const FName& InColumn, const FText& InValueText);
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const TSharedRef<FTraceTreeDashboardViewFactory::SRowWidget>& InRowWidget, const TSharedRef<IDashboardDataTreeViewEntry>& InRowData, const FName& InColumn) override;
		
		virtual TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<IDashboardDataTreeViewEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) override;

		virtual TSharedPtr<SWidget> OnConstructContextMenu() override;
		virtual FReply OnDataRowKeyInput(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) const override;

		virtual const TMap<FName, FTraceTreeDashboardViewFactory::FHeaderRowColumnData>& GetHeaderRowColumns() const override;
		virtual const TMap<FName, FTraceTreeDashboardViewFactory::FColumnData>& GetColumns() const override;

		virtual void SortTable() override;
		virtual TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& GetTreeItemsSource() override { return FullTree; }
		virtual bool ResetTreeData() override;

		void RecursiveSort(TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& OutTree, TFunctionRef<bool(const FSoundDashboardEntry&, const FSoundDashboardEntry&)> Predicate);
		void SortByPredicate(TFunctionRef<bool(const FSoundDashboardEntry&, const FSoundDashboardEntry&)> Predicate);

		TSharedRef<SWidget> MakeShowRecentlyStoppedSoundsWidget();

#if WITH_EDITOR
		TSharedRef<SWidget> MakeMuteSoloWidget();

		TSharedRef<SWidget> CreateMuteSoloButton(const TSharedRef<IDashboardDataTreeViewEntry>& InRowData,
			const FName& InColumn,
			TFunction<void(const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& /*InEntries*/)> MuteSoloToggleFunc,
			TFunctionRef<bool(const IDashboardDataTreeViewEntry& /*InEntry*/, const bool /*bInCheckChildren*/)> IsMuteSoloFunc);

		void ToggleMuteSoloEntries(const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& InEntries, const EMuteSoloMode InMuteSoloMode);

		TArray<TObjectPtr<UObject>> GetSelectedEditableAssets() const;
#endif // WITH_EDITOR

		bool SelectedItemsIncludesAnAsset() const;
		void PinSound();
		void UnpinSound();
		bool SelectionIncludesUnpinnedItem() const;

		void PinSelectedItems(const TSharedPtr<IDashboardDataTreeViewEntry>& Entry, const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& SelectedItems);
		void UnpinSelectedItems(const TSharedPtr<FPinnedSoundEntryWrapper>& PinnedWrapperEntry, const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& SelectedItems, const bool bSelectionContainsAssets);

		void MarkBranchAsPinned(const TSharedPtr<IDashboardDataTreeViewEntry> Entry, const bool bIsPinned);
		void InitPinnedItemEntries();
		void CreatePinnedEntry(TSharedPtr<IDashboardDataTreeViewEntry> Entry);
		void UpdatePinnedSection();

#if WITH_EDITOR
		void BrowseSoundAsset() const;
		void OpenSoundAsset() const;
#endif
		// @TODO UE-250399: Hide category pending to implement
		//void HideSound();

		static constexpr ESoundDashboardFilterFlags AllFilterFlags = 
			ESoundDashboardFilterFlags::MetaSound        |
			ESoundDashboardFilterFlags::SoundCue         |
			ESoundDashboardFilterFlags::ProceduralSource |
			ESoundDashboardFilterFlags::SoundWave        |
			ESoundDashboardFilterFlags::SoundCueTemplate |
			ESoundDashboardFilterFlags::Pinned;
			// @TODO UE-250399: Hide category pending to implement
			//| ESoundDashboardFilterFlags::Hidden;

		TSharedPtr<FPinnedSoundEntryWrapper> PinnedItemEntries;
		TArray<TSharedPtr<IDashboardDataTreeViewEntry>> FullTree;

		TSharedPtr<FUICommandList> CommandList;
		TSharedPtr<SWidget> SoundsFilterBar;
		TSharedPtr<SWidget> SoundsFilterBarButton;

		ESoundDashboardFilterFlags SelectedFilterFlags = AllFilterFlags;
		bool bIsPinnedCategoryFilterEnabled = true;
		bool bShowRecentlyStoppedSounds = false;
	};
} // namespace UE::Audio::Insights
