// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "Templates/UniquePtr.h"
#include "MetaHumanTakeData.h"
#include "SourcesData.h"
#include "CoreMinimal.h"
#include "Settings/EditorLoadingSavingSettings.h"

template<typename ItemType>
class STileView;

template<typename ItemType>
class STreeView;

class UFootageCaptureData;
class STextBlock;
class SExpandableArea;
class SSearchBox;
class SDockTab;
class SEditableTextBox;
class SPositiveActionButton;
class SSimpleComboButton;
class SWidget;
struct FPathPickerConfig;

//#define INGEST_UNIMPLEMENTED_UI		// Uncomment to show unfinished UI features
//#define SHOW_FILTERS_FOR_SOURCE_PATH
//#define SHOW_CAPTURE_SOURCE_TOOLBAR

#define CANCEL_BUTTON_FOR_INDIVIDUAL_TASKS
#define TARGET_PATH_PICKER


class FFootageTakeItem;
struct FFootageCaptureSource;
struct FFootageFolderTreeItem;
class UMetaHumanCaptureSource;

DECLARE_DELEGATE_OneParam(FOnTargetFolderAssetPathChanged, FText TargetFolderAssetPath)
DECLARE_DELEGATE_OneParam(FOnAutosaveAfterImportChanged, bool bAutosaveAfterImport)

class SFootageIngestWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFootageIngestWidget) :
		_OwnerTab()
	{}
		SLATE_ARGUMENT(TWeakPtr<SDockTab>, OwnerTab)
		SLATE_EVENT(FOnTargetFolderAssetPathChanged, OnTargetFolderAssetPathChanged)
		SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	bool UnqueueTake(TSharedPtr<FFootageTakeItem> Take, bool bCancelingSingleItem);
	void UnqueueTakes(TArray<TSharedPtr<FFootageTakeItem>> Takes);

	//called by CaptureManagerWidget after CaptureSourcesWidget has processed the event
	void OnCurrentCaptureSourceChanged(TSharedPtr<FFootageCaptureSource> InCaptureSource, ESelectInfo::Type InSelectInfo);

	//called by CaptureManagerWidget after CaptureSourcesWidget has processed the event
	void OnCaptureSourcesChanged(TArray<TSharedPtr<FFootageCaptureSource>> InCaptureSources);

	//called by CaptureManagerWidget after CaptureSourcesWidget has processed the event
	void OnCaptureSourceUpdated(TSharedPtr<FFootageCaptureSource> InCaptureSource);

	//called by CaptureManagerWidget after the CaptureSourcesWidget has processed the event
	void OnCaptureSourceFinishedImportingTakes(const TArray<FMetaHumanTake>& InTakes, TSharedPtr<FFootageCaptureSource> InCaptureSource);

	//called by CaptureManagerWidget when capture manager is closed
	bool CanClose();
	void OnClose();

	//called by CaptureManager after clicking on the check box
	void SetAutosaveAfterImport(bool bInAutosave) { bSaveAfterIngest = bInAutosave; };

	//called right after import if bSaveAfterIngest is on, or from clicking on CaptureManager's Save All button
	void SaveImportedAssets(); 
	void SetDefaultAssetCreationPath(const FString& InDefaultAssetCreationPath);

private:

	void OnTakeViewSelectionChanged(TSharedPtr<FFootageTakeItem> InTakeItem, ESelectInfo::Type InSelectInfo);
	void OnQueueListSelectionChanged(TSharedPtr<FFootageTakeItem> InTakeItem, ESelectInfo::Type InSelectInfo);

	void OnTakeFilterTextCommitted(const FText& InSearchText, ETextCommit::Type InCommitType);
	void OnTakeFilterTextChanged(const FText& InSearchText);

	FText GetTakeCountText() const;

	FReply OnImportTakesClicked();
	bool IsImportTakesEnabled() const;
	FReply OnCancelAllImportClicked();
	bool IsCancelAllImportEnabled() const;
	FReply OnClearAllImportClicked();
	bool IsClearAllImportEnabled() const;

	FReply OnQueueButtonClicked();
	bool IsQueueButtonEnabled() const;
	const FSlateBrush* GetQueueButtonIcon() const;
	FText GetQueueButtonText() const;
	FText GetQueueButtonTooltip() const;
	FText GetTargetFolderPickerPathTooltip() const;

	void SubscribeToCaptureSourceEvents(TSharedPtr<FFootageCaptureSource>& CaptureSource);

	void OnGetTakeImported(struct FMetaHumanCapturePerTakeVoidResult InResult);

	FOnTargetFolderAssetPathChanged OnTargetFolderAssetPathChangedDelegate;

private:

	static const FName CaptureSourcesTabId;
	static const FName FootageIngestTabId;

	bool IsCurrentCaptureSourceAssetValid() const;

	TSharedPtr<FFootageTakeItem> GetTakeItemById(FFootageCaptureSource& InCaptureSource, TakeId InTakeId);
	void UpdateTakeList(TSharedPtr<FFootageCaptureSource> InCaptureSource, const TArray<int32>& NewTakes);
	void RemoveFromTakeList(TSharedPtr<FFootageCaptureSource> InCaptureSource, const TArray<TakeId>& RemovedTakes);
	void UpdateThumbnail(FFootageCaptureSource& InCaptureSource, TakeId InTakeId);
	bool LoadThumbnail(const TArray<uint8>& InThumbnailRawData, TSharedPtr<FFootageTakeItem> InTakeItem);
	TArray<TSharedPtr<FFootageTakeItem>>& GetCurrentTakeList();
	void SetTakeViewListSource(TArray<TSharedPtr<FFootageTakeItem>>* InListSource);
	
	UFootageCaptureData* GetOrCreateCaptureData(const FString& InTargetIngestPath, const FString& InAssetName) const;
	UFootageCaptureData* GetCaptureData(const FString& InTargetIngestPath, const FString& InAssetName) const;
	TArray<FAssetData> GetAssetsInPath(const FString& InTargetIngestPath);

	bool CheckIfTakeShouldBeIngested(const FString& InSourceName, const TakeId InTakeId) const;
	bool PresentDialogForIngestedTakes(const TArray<TSharedPtr<FFootageTakeItem>>& InAlreadyIngestedTakes) const;

	void LoadAlreadyIngestedTakes(const TSharedPtr<FFootageCaptureSource> InCaptureSource);
	void CheckIfTakeIsAlreadyIngested(const TSharedPtr<FFootageTakeItem> InTake);

#ifdef TARGET_PATH_PICKER
	TSharedRef<SWidget> CreatePathPicker(const FPathPickerConfig& PathPickerConfig);
	void OnTargetPathChange(const FString& NewPath);
	TSharedRef<SWidget> GetPathPickerContent();
#endif// TARGET_PATH_PICKER

	TWeakPtr<SDockTab> OwnerTab = nullptr;

	// Widgets

	TSharedPtr<STileView<TSharedPtr<FFootageTakeItem>>> TakeTileView;
	TSharedPtr<SListView<TSharedPtr<FFootageCaptureSource>>> SourceListView;
	TSharedPtr<SListView<TSharedPtr<FFootageTakeItem>>> QueueListView;
	TSharedPtr<STreeView<TSharedPtr<FFootageFolderTreeItem>>> FolderTreeView;

	TSharedPtr<STextBlock> TakeStatusBarText;

	TSharedPtr<SEditableTextBox> TargetFolderTextBox;

	TSharedPtr<SExpandableArea> CaptureSourcesArea;
	TSharedPtr<SExpandableArea> DeviceContentsArea;

	TSharedPtr<SSearchBox> TakeSearchBar;

	TSharedPtr<SPositiveActionButton> AddToQueueButton;

	// Objects

	TArray<TSharedPtr<FFootageTakeItem>>* TakeViewListSource;
	TArray<TSharedPtr<FFootageTakeItem>> TakeItems_Null;
	TArray<TSharedPtr<FFootageTakeItem>> TakeItems_Filtered;
	TArray<TSharedPtr<FFootageTakeItem>> QueuedTakes;

	TArray<TSharedPtr<FFootageFolderTreeItem>> FolderTreeItemList;

	TArray<TSharedPtr<FFootageCaptureSource>> CaptureSources;
	TSharedPtr<FFootageCaptureSource> CurrentCaptureSource;

	// Misc.

	bool bImportingTakes;

	FText TakeFilterText;

	/** The asset path for the folder shown in the Target Folder Picker, relative to Content folder, begins with /Game/ and includes [CaptureSource]_Ingested */
	FText TargetFolderPickerAssetPath;
	/** The full directory path (on disk) picked in the Target Folder Picker, including the suffix to the Capture Source (_Ingested) */
	FText TargetFolderPickerFullPathOnDisk;

	bool bSaveAfterIngest; //set by CaptureManagerWidget on initialization and each toggle of Autosave checkbox
	TArray<FAssetData> AssetsToSave;

	TMap<FString, TArray<TakeId>> IngestedTakesCache;
	FString DefaultAssetCreationPath;

	void AddAutoReimportExemption(UEditorLoadingSavingSettings* Settings, FString DirectoryPath);
	static FString PathOnDiskFromAssetPath(const FString& InAssetPath);
	static FString GetDefaultAssetPath(const FName& InCaptureSourcePackageName);

#ifdef TARGET_PATH_PICKER
	TSharedPtr<SSimpleComboButton> PathPickerButton;
#endif// TARGET_PATH_PICKER
};