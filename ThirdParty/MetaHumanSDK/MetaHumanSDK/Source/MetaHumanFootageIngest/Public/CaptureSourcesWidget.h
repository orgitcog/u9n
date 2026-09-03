// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "DevelopersContentFilter.h"
#include "Templates/UniquePtr.h"
#include "MetaHumanTakeData.h"
#include "SourcesData.h"
#include "CoreMinimal.h"

#include "MetaHumanCaptureIngester.h"

class UFootageCaptureData;
class SExpandableArea;
class SDockTab;

struct FFootageCaptureSource;
class UMetaHumanCaptureSource;
class FAssetRegistryModule;

//#define INGEST_UNIMPLEMENTED_UI		// Uncomment to show unfinished UI features

enum class EFootageTakeItemStatus
{
	Unqueued,
	Queued,
	Warning,
	Ingest_Active,
	Ingest_Paused,
	Ingest_Canceled,
	Ingest_Failed,
	Ingest_Succeeded,
	Ingest_Succeeded_with_Warnings,
};

class FFootageTakeItem : public FGCObject
{
public:

	FText Name;
	TakeId TakeId;
	int32 NumFrames;
	FString PathToTakeFolder;
	TSharedPtr<FSlateBrush> PreviewImage;
	TObjectPtr<class UTexture2D> PreviewImageTexture = nullptr;
	bool PreviewSet = false;

	EFootageTakeItemStatus Status;
	FString StatusMessage;

	FText DestinationFolder;

	TSharedPtr<FFootageCaptureSource> CaptureSource;

	virtual ~FFootageTakeItem()
	{
		PreviewImage = nullptr;
		PreviewImageTexture = nullptr;
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(PreviewImageTexture);
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("FFootageTakeItem");
	}
};


enum class EFootageCaptureSourceStatus
{
	Closed,
	Offline,
	Online
};

struct FFootageCaptureSource
{
	explicit FFootageCaptureSource(UE::MetaHuman::FIngesterParams InIngesterParams);
	UE::MetaHuman::FIngester& GetIngester();

	FText Name;
	FString AssetPath;
	EFootageCaptureSourceStatus Status;
	bool bIsRecording = false;
	FString SlateName = TEXT("");
	int32 TakeNumber = 1;
	bool bImporting = false;
	FName PackageName;

	TArray<TSharedPtr<FFootageTakeItem>> TakeItems;

private:
	TUniquePtr<UE::MetaHuman::FIngester> Ingester;
};

struct FFootageFolderTreeItem
{
	FText Name;
	TArray<TSharedPtr<FFootageFolderTreeItem>> Children;
	TWeakPtr<FFootageFolderTreeItem> Parent;
};

DECLARE_DELEGATE_TwoParams(FOnCurrentCaptureSourceChanged, const TSharedPtr<FFootageCaptureSource>, ESelectInfo::Type InSelectInfo)
DECLARE_DELEGATE_OneParam(FOnCaptureSourcesChanged, const TArray<TSharedPtr<FFootageCaptureSource>>)
DECLARE_DELEGATE_OneParam(FOnCaptureSourceUpdated, const TSharedPtr<FFootageCaptureSource>)
DECLARE_DELEGATE_TwoParams(FOnCaptureSourceFinishedImportingTakes, const TArray<FMetaHumanTake>& InTakes, TSharedPtr<FFootageCaptureSource> InCaptureSource)
DECLARE_DELEGATE_OneParam(FOnCaptureSourceRemoved, TSharedPtr<FFootageCaptureSource>)

class UCaptureSourceSet : public UObject
{

public:
	TArray<TObjectPtr<FFootageCaptureSource>> CaptureSources;
};

class SCaptureSourcesWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SCaptureSourcesWidget) :
		_OwnerTab()
		{}
		SLATE_ARGUMENT(TWeakPtr<SDockTab>, OwnerTab)

		SLATE_EVENT(FOnCurrentCaptureSourceChanged, OnCurrentCaptureSourceChanged)
		SLATE_EVENT(FOnCaptureSourcesChanged, OnCaptureSourcesChanged)
		SLATE_EVENT(FOnCaptureSourceUpdated, OnCaptureSourceUpdated)
		SLATE_EVENT(FOnCaptureSourceFinishedImportingTakes, OnCaptureSourceFinishedImportingTakes)
	SLATE_END_ARGS()

	SCaptureSourcesWidget();
	~SCaptureSourcesWidget();

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void OnTargetFolderAssetPathChanged(FText InTargetFolderAssetPath);

	TArray<TSharedPtr<FFootageCaptureSource>> GetCaptureSources() { return CaptureSources; };

	void StartCaptureSources();
	void RefreshCurrentCaptureSource() const;

	const FFootageCaptureSource* GetCurrentCaptureSource() const;
	FFootageCaptureSource* GetCurrentCaptureSource();

	bool CanClose();
	void OnClose();

	bool IsShowingDevelopersContent() const;
	bool IsShowingOtherDevelopersContent() const;
	void ToggleShowDevelopersContent();
	void ToggleShowOtherDevelopersContent();
	
private:

	void OnCurrentCaptureSourceChanged(TSharedPtr<FFootageCaptureSource> InCaptureSource, ESelectInfo::Type InSelectInfo);
	void OnCaptureSourceFinishedImportingTakes(const TArray<FMetaHumanTake>& InTakes, TSharedPtr<FFootageCaptureSource> InCaptureSource);
	void OnCaptureSourcePropertyEvent(UObject* InObject, FPropertyChangedEvent& InEvent);

	UFootageCaptureData* GetOrCreateCaptureData(const FString& InTargetIngestPath, const FString& InAssetName) const;

	static const FName CaptureSourcesTabId;

	void InitCaptureSourceList();
	void LoadCaptureSources(const TArray<FAssetData>& InAssetDataCollection);
	void OnAssetAdded(const FAssetData& InAssetData);
	void OnAssetRemoved(const FAssetData& InAssetData);
	void OnAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath);
	void OnAssetUpdated(const FAssetData& InAssetData);
	void OnAssetReload(EPackageReloadPhase InPhase, FPackageReloadedEvent* InPackageEvent);
	bool IsCurrentCaptureSourceAssetValid() const;

	void UpdateCaptureSourceFilterSettings();
	void LoadCaptureSourceFilterFromSettings();
	void ToggleCaptureSourceFilterDevelopersContent();
	void ToggleCaptureSourceFilterShowOtherDevelopersContent();
	void FilterCaptureSourceList();

	TWeakPtr<SDockTab> OwnerTab = nullptr;

	// Widgets
	TSharedPtr<SListView<TSharedPtr<FFootageCaptureSource>>> SourceListView;

	TSharedPtr<SExpandableArea> CaptureSourcesArea;
	TSharedPtr<SExpandableArea> DeviceContentsArea;

	TArray<TSharedPtr<FFootageFolderTreeItem>> FolderTreeItemList;

	TArray<TSharedPtr<FFootageCaptureSource>> CaptureSources;
	TSharedPtr<FFootageCaptureSource> CurrentCaptureSource;

	//this delegate is passed from CaptureManagerWidget and invoked in OnCaptureSourceChanged of this class 
	//so other CaptureManager's tabs (FootageIngest) can react to the change AFTER it has been handled by CaptureSources list
	FOnCurrentCaptureSourceChanged OnCurrentCaptureSourceChangedDelegate;

	//this delegate is passed from CaptureManagerWidget and invoked in OnCaptureSourceChanged of this class 
	//so other CaptureManager's tabs (FootageIngest) can react to the change AFTER it has been handled by CaptureSources list
	FOnCaptureSourcesChanged OnCaptureSourcesChangedDelegate;

	//this delegate is passed from CaptureManagerWidget and invoked in OnCaptureSourceChanged of this class 
	//so other CaptureManager's tabs (FootageIngest) can react to the change AFTER it has been handled by CaptureSources list
	FOnCaptureSourceUpdated OnCaptureSourceUpdatedDelegate;

	//this delegate is passed from CaptureManagerWidget and invoked in OnCaptureSourceFinishedImportingTakesDelegate of this class 
	//so other CaptureManager's tabs (FootageIngest) can react to the change AFTER it has been handled by CaptureSources list
	FOnCaptureSourceFinishedImportingTakes OnCaptureSourceFinishedImportingTakesDelegate;

	/** the asset path for the folder picked in the Target Folder Picker of FootageIngestWidget */
	FText TargetFolderAssetPath;

	UE::MetaHuman::FDevelopersContentFilter DevelopersContentFilter;

	// We maintain a separate "view" into the capture sources list, to keep the management of the sources and the properties of the view separate
	TArray<TSharedPtr<FFootageCaptureSource>> FilteredCaptureSources;
};
