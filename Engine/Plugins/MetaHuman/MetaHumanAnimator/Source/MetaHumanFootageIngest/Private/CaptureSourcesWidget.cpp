// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourcesWidget.h"
#include "MetaHumanCaptureSource.h"
#include "MetaHumanEditorSettings.h"
#include "CaptureData.h"
#include "MetaHumanFootageRetrievalWindowStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SExpandableArea.h"

#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"

#include "UObject/ConstructorHelpers.h"

#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"

#include "Misc/Base64.h"
#include "Engine/Texture2D.h"
#include "UObject/PackageReload.h"
#include "Misc/MessageDialog.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "CaptureManagerLog.h"

#define LOCTEXT_NAMESPACE "CaptureSourcesWidget"

FFootageCaptureSource::FFootageCaptureSource(UE::MetaHuman::FIngesterParams InIngesterParams) :
	Ingester(MakeUnique<UE::MetaHuman::FIngester>(MoveTemp(InIngesterParams)))
{
	ensureAlways(Ingester);
}

UE::MetaHuman::FIngester& FFootageCaptureSource::GetIngester()
{
	return *Ingester;
}

class SFootageCaptureSourceRow : public STableRow<TSharedPtr<FFootageCaptureSource>>
{
public:
	SLATE_BEGIN_ARGS(SFootageCaptureSourceRow) {}
		SLATE_ARGUMENT(TSharedPtr<FFootageCaptureSource>, Item)
	SLATE_END_ARGS()

		static TSharedRef<ITableRow> BuildRow(TSharedPtr<FFootageCaptureSource> InItem, const TSharedRef<STableViewBase>& InOwnerTable)
	{
		if (!ensure(InItem.IsValid()))
		{
			return SNew(STableRow<TSharedPtr<FFootageCaptureSource>>, InOwnerTable);
		}

		return SNew(SFootageCaptureSourceRow, InOwnerTable).Item(InItem);
	}

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
	{
		check(InArgs._Item.IsValid());

		STableRow::Construct(
			STableRow::FArguments()
			.Padding(2)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(0.0, 2.0, 5.0, 2.0))
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(GetBrushBasedOnType(InArgs._Item))
				]
				+ SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Text(InArgs._Item->Name)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0)
				[
					SNew(SImage)
					.Image(this, &SFootageCaptureSourceRow::GetConnectedIndicatorBrush, InArgs._Item)
					.ToolTipText(this, &SFootageCaptureSourceRow::GetConnectedIndicatorTooltipText, InArgs._Item)
				]
			],
			InOwnerTable
		);
	}

private:

	const FSlateBrush* GetBrushBasedOnType(TSharedPtr<FFootageCaptureSource> InItem) const
	{
		if (InItem.IsValid())
		{
			switch (InItem->GetIngester().GetCaptureSourceType())
			{
				case EMetaHumanCaptureSourceType::LiveLinkFaceConnection:
					return FMetaHumanFootageRetrievalWindowStyle::Get().GetBrush("CaptureManager.DeviceTypeiPhone");
				case EMetaHumanCaptureSourceType::LiveLinkFaceArchives:
					return FMetaHumanFootageRetrievalWindowStyle::Get().GetBrush("CaptureManager.DeviceTypeiPhoneArchive");
				case EMetaHumanCaptureSourceType::HMCArchives:
					return FMetaHumanFootageRetrievalWindowStyle::Get().GetBrush("CaptureManager.DeviceTypeHMC");
				default:
					return FMetaHumanFootageRetrievalWindowStyle::Get().GetBrush("CaptureManager.DeviceTypeUnknown");
			}
		}

		return nullptr;
	}

	const FSlateBrush* GetConnectedIndicatorBrush(TSharedPtr<FFootageCaptureSource> InItem) const
	{
		if (InItem.IsValid())
		{
			if (InItem->Status == EFootageCaptureSourceStatus::Online)
			{
				if (InItem->bIsRecording)
				{
					return FMetaHumanFootageRetrievalWindowStyle::Get().GetBrush("CaptureManager.StartCapture");
				}
				else
				{
					return FMetaHumanFootageRetrievalWindowStyle::Get().GetBrush("CaptureManager.DeviceOnline");
				}
			}
		}
		return FMetaHumanFootageRetrievalWindowStyle::Get().GetBrush("CaptureManager.DeviceOffline");
	}

	FText GetConnectedIndicatorTooltipText(TSharedPtr<FFootageCaptureSource> InItem) const
	{
		if (InItem.IsValid())
		{
			if (InItem->Status == EFootageCaptureSourceStatus::Online)
			{
				return LOCTEXT("CaptureManagerDeviceOnlineTooltip", "This Capture Source is online");
			}
		}
		return LOCTEXT("CaptureManagerDeviceOfflineTooltip", "This Capture Source is offline");
	}
};

SCaptureSourcesWidget::SCaptureSourcesWidget():
	DevelopersContentFilter(UE::MetaHuman::EDevelopersContentVisibility::NotVisible, UE::MetaHuman::EOtherDevelopersContentVisibility::NotVisible)
{
}

SCaptureSourcesWidget::~SCaptureSourcesWidget() = default;

void SCaptureSourcesWidget::Construct(const FArguments& InArgs)
{
	OwnerTab = InArgs._OwnerTab;
	OnCurrentCaptureSourceChangedDelegate = InArgs._OnCurrentCaptureSourceChanged;
	OnCaptureSourcesChangedDelegate = InArgs._OnCaptureSourcesChanged;
	OnCaptureSourceUpdatedDelegate = InArgs._OnCaptureSourceUpdated;
	OnCaptureSourceFinishedImportingTakesDelegate = InArgs._OnCaptureSourceFinishedImportingTakes;

	//initially, the target path is empty because there is no capture source selected
	//the text box (breadcrumbs trail in future) is filled in in FootageIngestWidget::OnTargetPathChange

	ChildSlot
	[
		SNew(SVerticalBox)
		// Main pane
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0.0f)
		[
#ifdef INGEST_UNIMPLEMENTED_UI
			SNew(SSplitter)
			.PhysicalSplitterHandleSize(2.0f)

			+ SSplitter::Slot()
			.Value(0.15f)
			[
				SNew(SBox)
				.Padding(FMargin(4.f))
				[
					SNew(SBorder)
					.Padding(FMargin(0))
					.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
					[
						SNew(SSplitter)
						.Clipping(EWidgetClipping::ClipToBounds)
						.PhysicalSplitterHandleSize(2.0f)
						.HitDetectionSplitterHandleSize(8.0f)
						.Orientation(EOrientation::Orient_Vertical)
						.MinimumSlotHeight(26.0f)

						+ SSplitter::Slot()
						.SizeRule_Lambda([this]() { return CaptureSourcesArea->IsExpanded() ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent; })
						.Value(0.5f)
						[
							SAssignNew(CaptureSourcesArea, SExpandableArea)
							.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
							.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
							.HeaderPadding(FMargin(4.0f, 2.0f))
							.Padding(0)
							.AllowAnimatedTransition(false)
							.HeaderContent()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("CaptureSourcesHeader", "Capture Sources"))
									.TextStyle(FAppStyle::Get(), "ButtonText")
									.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
								]
							]
							.BodyContent()
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
								[
#endif // INGEST_UNIMPLEMENTED_UI
									SAssignNew(SourceListView, SListView<TSharedPtr<FFootageCaptureSource>>)
									.ScrollbarVisibility( EVisibility::Visible)
									.ListItemsSource(&FilteredCaptureSources)
									.SelectionMode(ESelectionMode::SingleToggle)
									.ClearSelectionOnClick(true)
									.OnGenerateRow_Static(&SFootageCaptureSourceRow::BuildRow)
									.OnSelectionChanged(this, &SCaptureSourcesWidget::OnCurrentCaptureSourceChanged)
#ifdef INGEST_UNIMPLEMENTED_UI
								]
							]
						]
						+ SSplitter::Slot()
						 .SizeRule_Lambda([this]() { return DeviceContentsArea->IsExpanded() ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent; })
						 .Value(0.5f)
						[
							SAssignNew(DeviceContentsArea, SExpandableArea)
							.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
							.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
							.HeaderPadding(FMargin(4.0f, 2.0f))
							.Padding(0)
							.AllowAnimatedTransition(false)
							.HeaderContent()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("DeviceContentsHeader", "Device Contents"))
									.TextStyle(FAppStyle::Get(), "ButtonText")
									.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
								]
							]
							.BodyContent()
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
								[
									SAssignNew(FolderTreeView, STreeView<TSharedPtr<FFootageFolderTreeItem>>)
									.TreeItemsSource(&FolderTreeItemList)
								]
							]
						]
					]

				]

			]
#endif //INGEST_UNIMPLEMENTED_UI
		]
	];

	LoadCaptureSourceFilterFromSettings();
	InitCaptureSourceList();
}

void SCaptureSourcesWidget::InitCaptureSourceList()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;
	const UClass* Class = UMetaHumanCaptureSource::StaticClass();
	AssetRegistryModule.Get().GetAssetsByClass(FTopLevelAssetPath(Class->GetPathName()), AssetData);

	// Set up delegates to respond to asset changes while the window is open.
	AssetRegistryModule.Get().OnAssetAdded().AddSP(this, &SCaptureSourcesWidget::OnAssetAdded);
	AssetRegistryModule.Get().OnAssetRemoved().AddSP(this, &SCaptureSourcesWidget::OnAssetRemoved);
	AssetRegistryModule.Get().OnAssetRenamed().AddSP(this, &SCaptureSourcesWidget::OnAssetRenamed);
	AssetRegistryModule.Get().OnAssetUpdated().AddSP(this, &SCaptureSourcesWidget::OnAssetUpdated);

	FCoreUObjectDelegates::OnPackageReloaded.AddSP(this, &SCaptureSourcesWidget::OnAssetReload);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SCaptureSourcesWidget::OnCaptureSourcePropertyEvent);

	LoadCaptureSources(AssetData);

	//this would be the place to trigger initial CaptureSourcesChanged event, so the FootageIngestWidget
	//can update its capture sources list, but since that widget is not yet created, and the creation
	//is done through TabManager, the call is moved to the only place we're sure both widgets exist:
	//CaptureManager->Show(), after main CaptureManager tab is invoked (by clicking the option in the
	//Window menu)
}

void SCaptureSourcesWidget::LoadCaptureSources(const TArray<FAssetData>& InAssetDataCollection)
{
	using namespace UE::MetaHuman;

	for (const FAssetData& AssetData : InAssetDataCollection)
	{
		if (UMetaHumanCaptureSource* Asset = Cast<UMetaHumanCaptureSource>(AssetData.GetAsset()))
		{
			TSharedPtr<FFootageCaptureSource> Src = MakeShared<FFootageCaptureSource>(
				FIngesterParams(
					Asset->CaptureSourceType,
					Asset->StoragePath,
					Asset->DeviceIpAddress,
					Asset->DeviceControlPort,
					Asset->ShouldCompressDepthFiles,
					Asset->CopyImagesToProject,
					Asset->MinDistance,
					Asset->MaxDistance,
					Asset->DepthPrecision,
					Asset->DepthResolution
				)
			);

			Src->Name = FText::FromString(AssetData.GetAsset()->GetName());
			Src->Status = EFootageCaptureSourceStatus::Closed;
			Src->PackageName = AssetData.PackageName;

			CaptureSources.Emplace(Src);
		}
	}

	FilterCaptureSourceList();
}

void SCaptureSourcesWidget::OnAssetAdded(const FAssetData& InAssetData)
{
	using namespace UE::MetaHuman;

	if (InAssetData.IsInstanceOf(UMetaHumanCaptureSource::StaticClass()))
	{
		// Check for duplicates in case we have renamed the source.
		for (int32 SourceIndex = 0; SourceIndex < CaptureSources.Num(); ++SourceIndex)
		{
			if (CaptureSources[SourceIndex]->PackageName == InAssetData.PackageName)
			{
				return;
			}
		}

		if (UMetaHumanCaptureSource* Asset = Cast<UMetaHumanCaptureSource>(InAssetData.GetAsset()))
		{
			TSharedPtr<FFootageCaptureSource> Src = MakeShared<FFootageCaptureSource>(
				FIngesterParams(
					Asset->CaptureSourceType,
					Asset->StoragePath,
					Asset->DeviceIpAddress,
					Asset->DeviceControlPort,
					Asset->ShouldCompressDepthFiles,
					Asset->CopyImagesToProject,
					Asset->MinDistance,
					Asset->MaxDistance,
					Asset->DepthPrecision,
					Asset->DepthResolution
				)
			);

			Src->Name = FText::FromString(InAssetData.GetAsset()->GetName());
			Src->Status = EFootageCaptureSourceStatus::Closed;
			Src->PackageName = InAssetData.PackageName;

			CaptureSources.Emplace(Src);
			OnCaptureSourcesChangedDelegate.ExecuteIfBound(CaptureSources);

			// Ingester startup needs to come after OnCaptureSourcesChanged, as event subscribers are added during that call, and the 
			// connectionChanged event is emitted during startup.
			Src->GetIngester().Startup();
			Src->GetIngester().OnGetTakesFinishedDelegate.AddSP(this, &SCaptureSourcesWidget::OnCaptureSourceFinishedImportingTakes, Src);

			FilterCaptureSourceList();
		}
	}
}

void SCaptureSourcesWidget::OnAssetRemoved(const FAssetData& InAssetData)
{
	if (InAssetData.IsInstanceOf(UMetaHumanCaptureSource::StaticClass()))
	{
		for (int32 Index = 0; Index < CaptureSources.Num(); ++Index)
		{
			// Check on PackageName as it is a unique identifier for the asset
			if (InAssetData.PackageName == CaptureSources[Index]->PackageName)
			{
				TSharedPtr<FFootageCaptureSource> DeletedSrc = CaptureSources[Index];
				CaptureSources.RemoveAt(Index);

				if (DeletedSrc == CurrentCaptureSource)
				{
					CurrentCaptureSource = nullptr;
				}

				break;
			}
		}

		// TODO remove queued takes from this source

		FilterCaptureSourceList();
		OnCaptureSourcesChangedDelegate.ExecuteIfBound(CaptureSources);
	}
}

void SCaptureSourcesWidget::OnAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath)
{
	// NOTE: If an asset is renamed, this is called first, FOLLOWED by OnAssetRemoved()
	// then OnAssetAssed(). Nothing will happen in OnAssetRemoved() because
	// that gets called with the old AssetData. OnAssetAdded() checks for duplicates, so
	// nothing will happen there either.

	if (InAssetData.IsInstanceOf(UMetaHumanCaptureSource::StaticClass()))
	{
		for (int32 Index = 0; Index < CaptureSources.Num(); Index++)
		{
			if (CaptureSources[Index]->PackageName == InAssetData.PackageName)
			{
				CaptureSources[Index]->Name = FText::FromString(InAssetData.GetAsset()->GetName());
			}
		}

		FilterCaptureSourceList();

		//notify the parent (CaptureManagerWidget) that the sources have changed
		//so it can pass the sources list to FootageIngestWidget
		OnCaptureSourcesChangedDelegate.ExecuteIfBound(CaptureSources);
	}
}

// Gets called on asset save (for example when the user changes max distance).
void SCaptureSourcesWidget::OnAssetUpdated(const FAssetData& InAssetData)
{
	using namespace UE::MetaHuman;

	if (InAssetData.IsInstanceOf(UMetaHumanCaptureSource::StaticClass()))
	{
		for (const TSharedPtr<FFootageCaptureSource>& CaptureSource : CaptureSources)
		{
			if (InAssetData.PackageName == CaptureSource->PackageName)
			{
				if (UMetaHumanCaptureSource* Asset = Cast<UMetaHumanCaptureSource>(InAssetData.GetAsset()))
				{
					CaptureSource->GetIngester().SetParams(
						FIngesterParams(
							Asset->CaptureSourceType,
							Asset->StoragePath,
							Asset->DeviceIpAddress,
							Asset->DeviceControlPort,
							Asset->ShouldCompressDepthFiles,
							Asset->CopyImagesToProject,
							Asset->MinDistance,
							Asset->MaxDistance,
							Asset->DepthPrecision,
							Asset->DepthResolution
						)
					);

					OnCaptureSourcesChangedDelegate.ExecuteIfBound(CaptureSources);

					// Ingester startup needs to come after OnCaptureSourcesChanged, as event subscribers are added during that call, and the 
					// connectionChanged event is emitted during startup.
					CaptureSource->GetIngester().Startup();
					break;
				}
			}
		}

		FilterCaptureSourceList();
	}
}

void SCaptureSourcesWidget::OnAssetReload(EPackageReloadPhase InPhase, FPackageReloadedEvent* InPackageEvent)
{
	using namespace UE::MetaHuman;

	if (InPhase == EPackageReloadPhase::PostPackageFixup)
	{
		const UPackage* OldPackage = InPackageEvent->GetOldPackage();
		if (!OldPackage)
		{
			return;
		}
		
		UObject* OldAsset = OldPackage->FindAssetInPackage();

		if (OldAsset && 
			UMetaHumanCaptureSource::StaticClass()->IsChildOf(OldAsset->GetClass()))
		{
			bool bShouldUpdate = false;

			for (TSharedPtr<FFootageCaptureSource> Src : CaptureSources)
			{
				if (Src->Name.ToString() == OldAsset->GetName())
				{
					const UPackage* NewPackage = InPackageEvent->GetNewPackage();

					if (UMetaHumanCaptureSource* Asset = Cast<UMetaHumanCaptureSource>(NewPackage->FindAssetInPackage()))
					{
						Src->GetIngester().SetParams(
							FIngesterParams(
								Asset->CaptureSourceType,
								Asset->StoragePath,
								Asset->DeviceIpAddress,
								Asset->DeviceControlPort,
								Asset->ShouldCompressDepthFiles,
								Asset->CopyImagesToProject,
								Asset->MinDistance,
								Asset->MaxDistance,
								Asset->DepthPrecision,
								Asset->DepthResolution
							)
						);
					}

					Src->Status = EFootageCaptureSourceStatus::Closed;
					Src->bIsRecording = false;

					OnCaptureSourceUpdatedDelegate.ExecuteIfBound(Src);

					Src->GetIngester().Startup();
					Src->GetIngester().OnGetTakesFinishedDelegate.AddSP(this, &SCaptureSourcesWidget::OnCaptureSourceFinishedImportingTakes, Src);

					bShouldUpdate = true;

					if (CurrentCaptureSource == Src)
					{
						CurrentCaptureSource = nullptr;
					}
				}
			}

			if (bShouldUpdate)
			{
				if (!CurrentCaptureSource)
				{
					SourceListView->ClearSelection();
				}

				FilterCaptureSourceList();
			}
		}
	}
}

void SCaptureSourcesWidget::OnCaptureSourcePropertyEvent(UObject* InObject, FPropertyChangedEvent& InEvent)
{
	using namespace UE::MetaHuman;

	if (!UMetaHumanCaptureSource::StaticClass()->IsChildOf(InObject->GetClass()))
	{
		return;
	}

	if (InEvent.ChangeType != EPropertyChangeType::ValueSet)
	{
		// Do nothing
		return;
	}

	TSharedPtr<FFootageCaptureSource> FoundSrc = nullptr;
	for (TSharedPtr<FFootageCaptureSource> Src : CaptureSources)
	{
		if (Src->Name.ToString() == InObject->GetName())
		{
			FoundSrc = Src;
			break;
		}
	}

	if (!FoundSrc.IsValid())
	{
		return;
	}

	FoundSrc->GetIngester().Shutdown();

	FoundSrc->Status = EFootageCaptureSourceStatus::Closed;
	FoundSrc->bIsRecording = false;

	FoundSrc->GetIngester().UnsubscribeAll();

	if (UMetaHumanCaptureSource* MetaHumanCaptureSource = Cast<UMetaHumanCaptureSource>(InObject))
	{
		FoundSrc->GetIngester().SetParams(
			FIngesterParams(
				MetaHumanCaptureSource->CaptureSourceType,
				MetaHumanCaptureSource->StoragePath,
				MetaHumanCaptureSource->DeviceIpAddress,
				MetaHumanCaptureSource->DeviceControlPort,
				MetaHumanCaptureSource->ShouldCompressDepthFiles,
				MetaHumanCaptureSource->CopyImagesToProject,
				MetaHumanCaptureSource->MinDistance,
				MetaHumanCaptureSource->MaxDistance,
				MetaHumanCaptureSource->DepthPrecision,
				MetaHumanCaptureSource->DepthResolution
			)
		);
	}

	OnCaptureSourceUpdatedDelegate.ExecuteIfBound(FoundSrc);

	FoundSrc->GetIngester().Startup();
	FoundSrc->GetIngester().OnGetTakesFinishedDelegate.AddSP(this, &SCaptureSourcesWidget::OnCaptureSourceFinishedImportingTakes, FoundSrc);

	if (CurrentCaptureSource == FoundSrc)
	{
		SourceListView->ClearSelection();
	}

	if (InEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaHumanCaptureSource, CaptureSourceType))
	{
		SourceListView->RebuildList();
	}
}

bool SCaptureSourcesWidget::IsCurrentCaptureSourceAssetValid() const
{
	return CurrentCaptureSource.IsValid();
}

void SCaptureSourcesWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SCaptureSourcesWidget::OnCurrentCaptureSourceChanged(TSharedPtr<FFootageCaptureSource> InCaptureSource, ESelectInfo::Type InSelectInfo)
{
	CurrentCaptureSource = InCaptureSource;

	if (CurrentCaptureSource.IsValid() && CurrentCaptureSource->TakeItems.Num() == 0)
	{
		RefreshCurrentCaptureSource();
	}

	//notify the owner (CaptureManagerWidget) so its tabs (FootageIngest etc) can react to the change
	OnCurrentCaptureSourceChangedDelegate.ExecuteIfBound(InCaptureSource, InSelectInfo);
}

void SCaptureSourcesWidget::OnCaptureSourceFinishedImportingTakes(const TArray<FMetaHumanTake>& InTakes, TSharedPtr<FFootageCaptureSource> InCaptureSource)
{

	OnCaptureSourceFinishedImportingTakesDelegate.ExecuteIfBound(InTakes, InCaptureSource);

}

UFootageCaptureData* SCaptureSourcesWidget::GetOrCreateCaptureData(const FString& InTargetIngestPath, const FString& InAssetName) const
{
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	const FString AssetPackagePath = InTargetIngestPath / InAssetName;

	TArray<FAssetData> AssetData;
	AssetRegistry.GetAssetsByPackageName(FName{ *AssetPackagePath }, AssetData);

	if (!AssetData.IsEmpty())
	{
		return Cast<UFootageCaptureData>(AssetData[0].GetAsset());
	}
	else
	{
		return Cast<UFootageCaptureData>(AssetTools.CreateAsset(InAssetName, InTargetIngestPath, UFootageCaptureData::StaticClass(), nullptr));
	}
}

void SCaptureSourcesWidget::OnTargetFolderAssetPathChanged(FText InTargetFolderAssetPath)
{
	this->TargetFolderAssetPath = InTargetFolderAssetPath;
}

void SCaptureSourcesWidget::StartCaptureSources()
{
	for (TSharedPtr<FFootageCaptureSource> Src : CaptureSources)
	{
		Src->GetIngester().Startup();
		Src->GetIngester().OnGetTakesFinishedDelegate.AddSP(this, &SCaptureSourcesWidget::OnCaptureSourceFinishedImportingTakes, Src);
	}
}

void SCaptureSourcesWidget::RefreshCurrentCaptureSource() const
{
	using namespace UE::MetaHuman;

	if (CurrentCaptureSource.IsValid() && CurrentCaptureSource->Status == EFootageCaptureSourceStatus::Online)
	{
		if (CurrentCaptureSource->GetIngester().CanStartup())
		{
			CurrentCaptureSource->GetIngester().Refresh(FIngester::FRefreshCallback());
		}
		else
		{
			UE_LOG(LogCaptureManager, Error, TEXT("Could not start up Capture Source '%s'"), *CurrentCaptureSource->Name.ToString());
		}
	}
}

const FFootageCaptureSource* SCaptureSourcesWidget::GetCurrentCaptureSource() const
{
	return CurrentCaptureSource.Get();
}

FFootageCaptureSource* SCaptureSourcesWidget::GetCurrentCaptureSource()
{
	return const_cast<FFootageCaptureSource*>(const_cast<const SCaptureSourcesWidget*>(this)->GetCurrentCaptureSource());
}

bool SCaptureSourcesWidget::CanClose()
{
	bool bIsRecording = false;

	TArray<FString> CaptureSourceNames;
	for (TSharedPtr<FFootageCaptureSource>& CaptureSource : CaptureSources)
	{
		if (CaptureSource->bIsRecording)
		{
			CaptureSourceNames.Add(CaptureSource->Name.ToString());
		}

		bIsRecording |= CaptureSource->bIsRecording;
	}

	if (bIsRecording)
	{
		FTextBuilder TextBuilder;

		TextBuilder.AppendLine(LOCTEXT("CaptureSourcesIsRecordingDialog_Text", "Some of the sources are still recording and will be stopped."));
		TextBuilder.AppendLine(); // New line

		TextBuilder.AppendLine(LOCTEXT("CaptureSourcesIsRecordingDialog_Takes", "Sources that are recording:"));
		TextBuilder.Indent();

		for (const FString& CaptureSourceName : CaptureSourceNames)
		{
			TextBuilder.AppendLine(CaptureSourceName);
		}

		TextBuilder.Unindent();
		TextBuilder.AppendLine(); // New line
		TextBuilder.AppendLine(LOCTEXT("CaptureSourcesIsRecordingDialog_Question", "Are you sure you want to continue?"));

		EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNo,
															 TextBuilder.ToText());

		return Response == EAppReturnType::Yes;
	}

	return true;
}

void SCaptureSourcesWidget::OnClose()
{
	for (TSharedPtr<FFootageCaptureSource>& CaptureSource : CaptureSources)
	{
		if (CaptureSource.IsValid())
		{
			CaptureSource->GetIngester().Shutdown();
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	// Remove Footage Ingest asset delegates when window is closed
	AssetRegistryModule.Get().OnAssetAdded().RemoveAll(this);
	AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);
	AssetRegistryModule.Get().OnAssetRenamed().RemoveAll(this);
	AssetRegistryModule.Get().OnAssetUpdated().RemoveAll(this);

	FCoreUObjectDelegates::OnPackageReloaded.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
}

bool SCaptureSourcesWidget::IsShowingDevelopersContent() const
{
	return DevelopersContentFilter.GetDevelopersContentVisibility() == UE::MetaHuman::EDevelopersContentVisibility::Visible;
}

bool SCaptureSourcesWidget::IsShowingOtherDevelopersContent() const
{
	return DevelopersContentFilter.GetOtherDevelopersContentVisibility() == UE::MetaHuman::EOtherDevelopersContentVisibility::Visible;
}

void SCaptureSourcesWidget::ToggleShowDevelopersContent()
{
	ToggleCaptureSourceFilterDevelopersContent();
	UpdateCaptureSourceFilterSettings();
	FilterCaptureSourceList();
}

void SCaptureSourcesWidget::ToggleShowOtherDevelopersContent()
{
	ToggleCaptureSourceFilterShowOtherDevelopersContent();
	UpdateCaptureSourceFilterSettings();
	FilterCaptureSourceList();
}

void SCaptureSourcesWidget::UpdateCaptureSourceFilterSettings()
{
	using namespace UE::MetaHuman;

	TObjectPtr<UMetaHumanEditorSettings> MetaHumanEditorSettings = GetMutableDefault<UMetaHumanEditorSettings>();

	if (MetaHumanEditorSettings)
	{
		MetaHumanEditorSettings->bShowDevelopersContent = DevelopersContentFilter.GetDevelopersContentVisibility() == EDevelopersContentVisibility::Visible;
		MetaHumanEditorSettings->bShowOtherDevelopersContent = DevelopersContentFilter.GetOtherDevelopersContentVisibility() == EOtherDevelopersContentVisibility::Visible;
		MetaHumanEditorSettings->SaveConfig();
	}
	else
	{
		ensureMsgf(false, TEXT("Could not find the metahuman editor settings, unable to save capture source filter settings"));
	}
}

void SCaptureSourcesWidget::LoadCaptureSourceFilterFromSettings()
{
	using namespace UE::MetaHuman;

	TObjectPtr<UMetaHumanEditorSettings> MetaHumanEditorSettings = GetMutableDefault<UMetaHumanEditorSettings>();

	if (MetaHumanEditorSettings)
	{
		const EDevelopersContentVisibility DevelopersContentVisibility = MetaHumanEditorSettings->bShowDevelopersContent ? EDevelopersContentVisibility::Visible : EDevelopersContentVisibility::NotVisible;
		const EOtherDevelopersContentVisibility OtherDevelopersContentVisibility = MetaHumanEditorSettings->bShowOtherDevelopersContent ? EOtherDevelopersContentVisibility::Visible : EOtherDevelopersContentVisibility::NotVisible;
		DevelopersContentFilter = FDevelopersContentFilter(DevelopersContentVisibility, OtherDevelopersContentVisibility);
	}
	else
	{
		ensureMsgf(false, TEXT("Could not find the MetaHuman editor settings, default capture source filter settings will be used"));
		DevelopersContentFilter = FDevelopersContentFilter(EDevelopersContentVisibility::NotVisible, EOtherDevelopersContentVisibility::NotVisible);
	}
}

void SCaptureSourcesWidget::ToggleCaptureSourceFilterDevelopersContent()
{
	using namespace UE::MetaHuman;

	const EOtherDevelopersContentVisibility OtherDevelopersContentVisibility = DevelopersContentFilter.GetOtherDevelopersContentVisibility();

	if (DevelopersContentFilter.GetDevelopersContentVisibility() == EDevelopersContentVisibility::Visible)
	{
		DevelopersContentFilter = FDevelopersContentFilter(EDevelopersContentVisibility::NotVisible, OtherDevelopersContentVisibility);
	}
	else
	{
		DevelopersContentFilter = FDevelopersContentFilter(EDevelopersContentVisibility::Visible, OtherDevelopersContentVisibility);
	}
}

void SCaptureSourcesWidget::ToggleCaptureSourceFilterShowOtherDevelopersContent()
{
	using namespace UE::MetaHuman;

	const EDevelopersContentVisibility DevelopersContentVisibility = DevelopersContentFilter.GetDevelopersContentVisibility();

	if (DevelopersContentFilter.GetOtherDevelopersContentVisibility() == EOtherDevelopersContentVisibility::Visible)
	{
		DevelopersContentFilter = FDevelopersContentFilter(DevelopersContentVisibility, EOtherDevelopersContentVisibility::NotVisible);
	}
	else
	{
		DevelopersContentFilter = FDevelopersContentFilter(DevelopersContentVisibility, EOtherDevelopersContentVisibility::Visible);
	}
}

void SCaptureSourcesWidget::FilterCaptureSourceList()
{
	FilteredCaptureSources.Empty();

	for (const TSharedPtr<FFootageCaptureSource>& FootageCaptureSource : CaptureSources)
	{
		if (FootageCaptureSource)
		{
			if (DevelopersContentFilter.PassesFilter(*FootageCaptureSource->PackageName.ToString()))
			{
				FilteredCaptureSources.Emplace(FootageCaptureSource);
			}
		}
	}

	// Keep the filtered list in alphabetical order
	FilteredCaptureSources.Sort(
		[](const TSharedPtr<const FFootageCaptureSource>& Left, const TSharedPtr<const FFootageCaptureSource>& Right)
		{
			checkf(Left, TEXT("Filtered capture sources can not be sorted, 'Left' is nullptr"));
			checkf(Right, TEXT("Filtered capture sources can not be sorted, 'Right' is nullptr"));

			return Left->Name.CompareTo(Right->Name) < 0;
		}
	);

	ensureMsgf(SourceListView, TEXT("Capture source list view is nullptr"));

	if (SourceListView)
	{
		SourceListView->RebuildList();
	}
}

#undef LOCTEXT_NAMESPACE
