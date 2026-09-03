// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorBlendToolPanel.h"

#include "Algo/RemoveIf.h"
#include "AssetThumbnail.h"
#include "AssetToolsModule.h"
#include "ContentBrowserItem.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "MetaHumanCharacterEditorSettings.h"
#include "MetaHumanCharacterAssetObserver.h"
#include "MetaHumanCharacterEditorWardrobeSettings.h"
#include "MetaHumanCharacterEditorModule.h"
#include "Slate/DeferredCleanupSlateBrush.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorBlendToolPanel"

void SMetaHumanCharacterEditorBlendToolThumbnail::Construct(const FArguments& InArgs, int32 InItemIndex)
{
	OnItemDroppedDelegate = InArgs._OnItemDropped;
	OnItemDeletedDelegate = InArgs._OnItemDeleted;
	
	ItemIndex = InItemIndex;

	DefaultBrush = FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.Rounded.DefaultBrush");
	SelectedBrush = FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.Rounded.SelectedBrush");

	ThumbnailPool = MakeShared<FAssetThumbnailPool>(128);
	AssetThumbnail = MakeShared<FAssetThumbnail>(nullptr, 112.f, 112.f, ThumbnailPool);

	FAssetThumbnailConfig ThumbnailConfig;
	ThumbnailConfig.ThumbnailLabel = EThumbnailLabel::AssetName;

	ChildSlot
		[
			SNew(SBorder)
			.BorderImage(this, &SMetaHumanCharacterEditorBlendToolThumbnail::GetBorderBrush)
			.Padding(2.f)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(100.f)
					.WidthOverride(100.f)
					[
						SNew(SOverlay)

						// Thumbnail main section
						+SOverlay::Slot()
						[
							SAssignNew(ThumbnailContainerBox, SBox)
							[
								AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig)
							]
						]
							
						// Thumbnail overlay section
						+ SOverlay::Slot()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Top)
						[
							SNew(SVerticalBox)

							// Thumbnail delete button section
							+SVerticalBox::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Top)
							.AutoHeight()
							[
								SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
								.OnClicked(this, &SMetaHumanCharacterEditorBlendToolThumbnail::OnDeleteButtonClicked)
								.Visibility(this, &SMetaHumanCharacterEditorBlendToolThumbnail::GetDeleteButtonVisibility)
								[
									SNew(SImage)
									.Image(FAppStyle::Get().GetBrush("Icons.X"))
								]
							]
						]
					]
				]

				// Thumbnail Label section
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(0.f, 4.f, 0.f, 0.f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(this, &SMetaHumanCharacterEditorBlendToolThumbnail::GetThumbnailNameAsText)
					.Font(FAppStyle::GetFontStyle("ContentBrowser.AssetTileViewNameFont"))
					.OverflowPolicy(ETextOverflowPolicy::MultilineEllipsis)
				]
			]
		];
}

FAssetData SMetaHumanCharacterEditorBlendToolThumbnail::GetThumbnailAssetData() const
{
	FAssetData AssetData;
	if (AssetThumbnail.IsValid())
	{
		AssetData = AssetThumbnail->GetAssetData();
	}

	return AssetData;
}

void SMetaHumanCharacterEditorBlendToolThumbnail::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bIsDragging = true;

	ChildSlot.SetPadding(FMargin(-2.f));
}

void SMetaHumanCharacterEditorBlendToolThumbnail::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	bIsDragging = false;

	ChildSlot.SetPadding(FMargin(0.f));
}

FReply SMetaHumanCharacterEditorBlendToolThumbnail::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bIsDragging = false;

    TSharedPtr<FMetaHumanCharacterAssetViewItemDragDropOp> AssetDragDropOperation = DragDropEvent.GetOperationAs<FMetaHumanCharacterAssetViewItemDragDropOp>();
	if (!AssetDragDropOperation.IsValid() || !AssetThumbnail.IsValid() || !ThumbnailContainerBox.IsValid())
	{
		return FReply::Handled();
	}

	const TSharedPtr<FMetaHumanCharacterAssetViewItem>& AssetItem = AssetDragDropOperation->AssetItem;
	if (!AssetItem.IsValid())
	{
		return FReply::Handled();
	}

	const FAssetData& DroppedAssetData = AssetItem->AssetData;
	AssetThumbnail->SetAsset(DroppedAssetData);
	if (AssetItem->ThumbnailImageOverride.IsValid())
	{
		ThumbnailContainerBox->SetContent(GenerateThumbnailWidget(AssetItem));
	}
	else
	{
		AssetThumbnail->SetRealTime(true);
		AssetThumbnail->RefreshThumbnail();

		ThumbnailContainerBox->SetContent(AssetThumbnail->MakeThumbnailWidget());
	}

	OnItemDroppedDelegate.ExecuteIfBound(MyGeometry, DragDropEvent, ItemIndex);
	return FReply::Handled();
}

TSharedRef<SWidget> SMetaHumanCharacterEditorBlendToolThumbnail::GenerateThumbnailWidget(TSharedPtr<FMetaHumanCharacterAssetViewItem> AssetItem)
{
	TSharedPtr<SWidget> ThumbnailWidget =
		SNew(SImage)
		.Image(FAppStyle::GetDefaultBrush());

	if (!AssetItem.IsValid() || !AssetItem->ThumbnailImageOverride.IsValid())
	{
		return ThumbnailWidget.ToSharedRef();
	}

	const UObject* AssetItemObject = AssetItem->AssetData.GetAsset();

	if (IsValid(AssetItemObject))
	{
		const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(AssetItemObject->GetClass()).Pin();
		FLinearColor AssetColor = FLinearColor::White;
		if (AssetTypeActions.IsValid())
		{
			AssetColor = AssetTypeActions->GetTypeColor();
		}

		ThumbnailWidget =
			SNew(SOverlay)
			
			// Thumbnail image section
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image_Lambda([AssetItem]() -> const FSlateBrush*
				{
					if (AssetItem.IsValid())
					{
						return FDeferredCleanupSlateBrush::TrySlateBrush(AssetItem->ThumbnailImageOverride);
					}

					return nullptr;
				})
			]

			// Color strip section
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			[
				SNew(SBox)
				.HeightOverride(2.f)
				.Padding(1.8f, 0.f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(AssetColor)
				]
			];
	}

	return ThumbnailWidget.ToSharedRef();
}

const FSlateBrush* SMetaHumanCharacterEditorBlendToolThumbnail::GetBorderBrush() const
{
	return IsHovered() || bIsDragging ? SelectedBrush : DefaultBrush;
}

FText SMetaHumanCharacterEditorBlendToolThumbnail::GetThumbnailNameAsText() const
{
	if (AssetThumbnail.IsValid())
	{
		const FAssetData& AssetData = AssetThumbnail->GetAssetData();
		return IsValid(AssetData.GetAsset()) ? FText::FromName(AssetData.AssetName) : FText::FromString(TEXT("None"));
	}

	return FText::FromString(TEXT("None"));
}

EVisibility SMetaHumanCharacterEditorBlendToolThumbnail::GetDeleteButtonVisibility() const
{
	return AssetThumbnail.IsValid() && IsValid(AssetThumbnail->GetAsset()) ? EVisibility::Visible : EVisibility::Hidden;
}

FReply SMetaHumanCharacterEditorBlendToolThumbnail::OnDeleteButtonClicked()
{
	if (AssetThumbnail.IsValid())
	{
		AssetThumbnail->SetAsset(nullptr);
		AssetThumbnail->RefreshThumbnail();

		ThumbnailContainerBox->SetContent(AssetThumbnail->MakeThumbnailWidget());
	}
	OnItemDeletedDelegate.ExecuteIfBound(ItemIndex);
	return FReply::Handled();
}

void SMetaHumanCharacterEditorBlendToolPanel::Construct(const FArguments& InArgs, UMetaHumanCharacter* InCharacter)
{
	VirtualFolderSlotName = InArgs._VirtualFolderSlotName;
	OnItemActivatedDelegate = InArgs._OnItemActivated;
	OnOverrideItemThumbnailDelegate = InArgs._OnOverrideItemThumbnail;
	OnFilterAssetDataDelegate = InArgs._OnFilterAssetData;

	CharacterWeakPtr = InCharacter;

	UMetaHumanCharacterEditorSettings* MetaHumanEditorSettings = GetMutableDefault<UMetaHumanCharacterEditorSettings>();
	if (!MetaHumanEditorSettings->GetOnPresetsDirectoriesChanged().IsBoundToObject(this))
	{
		MetaHumanEditorSettings->GetOnPresetsDirectoriesChanged().BindSP(this, &SMetaHumanCharacterEditorBlendToolPanel::OnPresetsDirectoriesChanged);
	}

	const auto CreateBlendToolThumbnail = [this, InArgs](int32 InItemIndex)
		{ 
			const TSharedRef<SMetaHumanCharacterEditorBlendToolThumbnail> BlendToolThumbnail =
				SNew(SMetaHumanCharacterEditorBlendToolThumbnail, InItemIndex)
				.OnItemDropped(InArgs._OnItemDropped)
				.OnItemDeleted(InArgs._OnItemDeleted);

			BlendToolThumbnails.Add(BlendToolThumbnail);
			return BlendToolThumbnail;
		};

	ChildSlot
		[
			SNew(SVerticalBox)

			// Blend Tool Thumbnails section
			+SVerticalBox::Slot()
			.Padding(20.f)
			.AutoHeight()
			[
				SNew(SOverlay)

				// Background image section
				+SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(250.f, 250.f))
					.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.BlendTool.Circle")))
				]

				// Thumbnails section
				+ SOverlay::Slot()
				.Padding(4.f)
				[
					SNew(SConstraintCanvas)

					+SConstraintCanvas::Slot()
					.Anchors(FAnchors(0.5f))
					.Offset(FMargin(0.f, -80.f))
					.AutoSize(true)
					[
						SNew(SBox)
						.WidthOverride(100.f)
						.HeightOverride(130.f)
						[
							CreateBlendToolThumbnail(0)
						]
					]

					+ SConstraintCanvas::Slot()
					.Anchors(FAnchors(0.5f))
					.Offset(FMargin(-90.f, 70.f))
					.AutoSize(true)
					[
						SNew(SBox)
						.WidthOverride(100.f)
						.HeightOverride(130.f)
						[
							CreateBlendToolThumbnail(1)
						]
					]

					+ SConstraintCanvas::Slot()
					.Anchors(FAnchors(0.5f))
					.Offset(FMargin(90.f, 70.f))
					.AutoSize(true)
					[
						SNew(SBox)
						.WidthOverride(100.f)
						.HeightOverride(130.f)
						[
							CreateBlendToolThumbnail(2)
						]
					]
				]
			]

			// Presets View section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(AssetViewsPanel, SMetaHumanCharacterEditorAssetViewsPanel)
				.AllowDragging(true)
				.AllowSlots(false)
				.AllowMultiSelection(false)
				.AllowSlotMultiSelection(false)
				.AssetViewSections(this, &SMetaHumanCharacterEditorBlendToolPanel::GetAssetViewsSections)
				.ExcludedObjects({ InCharacter })
				.VirtualFolderClassesToFilter({ UMetaHumanCharacter::StaticClass() })
				.OnPopulateAssetViewsItems(this, &SMetaHumanCharacterEditorBlendToolPanel::OnPopulateAssetViewsItems)
				.OnProcessDroppedFolders(this, &SMetaHumanCharacterEditorBlendToolPanel::OnProcessDroppedFolders)
				.OnItemDeleted(this, &SMetaHumanCharacterEditorBlendToolPanel::OnBlendToolVirtualItemDeleted)
				.CanDeleteItem(this, &SMetaHumanCharacterEditorBlendToolPanel::CanDeleteBlendToolVirtualItem)
				.OnFolderDeleted(this, &SMetaHumanCharacterEditorBlendToolPanel::OnPresetsPathsFolderDeleted)
				.CanDeleteFolder(this, &SMetaHumanCharacterEditorBlendToolPanel::CanDeletePresetsPathsFolder)
				.OnHadleVirtualItem(this, &SMetaHumanCharacterEditorBlendToolPanel::OnHandleBlendVirtualItem)
				.OnItemActivated(OnItemActivatedDelegate)
				.OnOverrideThumbnail(OnOverrideItemThumbnailDelegate)

			]
		];
}

TArray<FAssetData> SMetaHumanCharacterEditorBlendToolPanel::GetBlendableItems() const
{
	TArray<FAssetData> ItemsData;
	for (const TSharedPtr<SMetaHumanCharacterEditorBlendToolThumbnail>& BlendToolThumbnail : BlendToolThumbnails)
	{
		if (!BlendToolThumbnail.IsValid())
		{
			continue;
		}

		const FAssetData AssetData = BlendToolThumbnail->GetThumbnailAssetData();
		if (AssetData.IsValid())
		{
			ItemsData.Add(AssetData);
		}
	}

	return ItemsData;
}

TArray<FMetaHumanCharacterAssetViewItem> SMetaHumanCharacterEditorBlendToolPanel::GetCharacterIndividualAssets() const
{
	TArray<FMetaHumanCharacterAssetViewItem> Items;

	UMetaHumanCharacter* Character = CharacterWeakPtr.IsValid() ? CharacterWeakPtr.Get() : nullptr;
	if (!Character)
	{
		return Items;
	}

	const FMetaHumanCharacterIndividualAssets* IndividualAssets = Character->CharacterIndividualAssets.Find(VirtualFolderSlotName);
	if (!IndividualAssets)
	{
		return Items;
	}

	for (TSoftObjectPtr<UMetaHumanCharacter> Item : IndividualAssets->Characters)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		const FAssetData AssetData = FAssetData(Item.Get());
		const FMetaHumanCharacterAssetViewItem AssetItem(AssetData, NAME_None, FMetaHumanPaletteItemKey(), nullptr);
		Items.Add(AssetItem);
	}

	// Sort assets by name
	Items.Sort([](const FMetaHumanCharacterAssetViewItem& ItemA, const FMetaHumanCharacterAssetViewItem& ItemB)
		{
			return ItemA.AssetData.AssetName.Compare(ItemB.AssetData.AssetName) < 0;
		});

	return Items;
}

TArray<FMetaHumanCharacterAssetsSection> SMetaHumanCharacterEditorBlendToolPanel::GetAssetViewsSections() const
{
	TArray<FMetaHumanCharacterAssetsSection> Sections;

	auto MakeSection = [](const FDirectoryPath& PathToMonitor)
	{
		const TArray<TSubclassOf<UObject>> ClassesToFiler = { UMetaHumanCharacter::StaticClass() };

		FMetaHumanCharacterAssetsSection Section;
		Section.ClassesToFilter = ClassesToFiler;
		Section.ContentDirectoryToMonitor = PathToMonitor;
		Section.SlotName = NAME_None;

		return Section;
	};

	// Append preset directories from the wardrobe settings
	if (FMetaHumanCharacterEditorModule::IsOptionalMetaHumanContentInstalled())
	{
		const UMetaHumanCharacterEditorWardrobeSettings* Settings = GetDefault<UMetaHumanCharacterEditorWardrobeSettings>();
		for (const FDirectoryPath& Path : Settings->PresetDirectories)
		{
			Sections.AddUnique(MakeSection(Path));
		}
	}

	// Append user sections from project settings
	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();
	for (const FDirectoryPath& Path : Settings->PresetsDirectories)
	{
		Sections.AddUnique(MakeSection(Path));
	}

	// Filter valid section settings
	return Sections.FilterByPredicate([](const FMetaHumanCharacterAssetsSection& Section)
		{
			FString LongPackageName;

			// Check if we provided the long package name
			if (!FPackageName::TryConvertLongPackageNameToFilename(
				Section.ContentDirectoryToMonitor.Path,
				LongPackageName))
			{
				return false;
			}

			if (Section.ClassesToFilter.IsEmpty())
			{
				return false;
			}

			return true;
		});
}

TArray<FMetaHumanCharacterAssetViewItem> SMetaHumanCharacterEditorBlendToolPanel::OnPopulateAssetViewsItems(const FMetaHumanCharacterAssetsSection& InSection, const FMetaHumanObserverChanges& InChanges)
{
	TArray<FMetaHumanCharacterAssetViewItem> Items;
 
	if (InSection.ContentDirectoryToMonitor.Path == TEXT("Individual Assets"))
	{
		Items.Append(GetCharacterIndividualAssets());
		return Items;
	}

	TArray<FAssetData> FoundAssets;
	FMetaHumanCharacterAssetObserver::Get().GetAssets(
		FName(InSection.ContentDirectoryToMonitor.Path),
		TSet(InSection.ClassesToFilter),
		FoundAssets);

	// Sort assets by name
	FoundAssets.Sort([](const FAssetData& AssetA, const FAssetData& AssetB)
		{
			return AssetA.AssetName.Compare(AssetB.AssetName) < 0;
		});

	for (const FAssetData& Asset : FoundAssets)
	{
		bool bFilterAsset = false;
		if (OnFilterAssetDataDelegate.IsBound())
		{
			bFilterAsset = OnFilterAssetDataDelegate.Execute(Asset);
		}

		if (!bFilterAsset)
		{
			Items.Add(FMetaHumanCharacterAssetViewItem(Asset, InSection.SlotName, FMetaHumanPaletteItemKey(), nullptr));
		}
	}

	return Items;
}

void SMetaHumanCharacterEditorBlendToolPanel::OnProcessDroppedFolders(const TArray<FContentBrowserItem> Items, const FMetaHumanCharacterAssetsSection& InSection) const
{
	UMetaHumanCharacterEditorSettings* MetaHumanEditorSettings = GetMutableDefault<UMetaHumanCharacterEditorSettings>();
	if (!MetaHumanEditorSettings || Items.IsEmpty())
	{
		return;
	}

	for (const FContentBrowserItem& Item : Items)
	{
		if (!Item.IsFolder())
		{
			continue;
		}

		const FString Path = Item.GetInternalPath().ToString();
		const bool bAlreadyContinsPath =
			MetaHumanEditorSettings->PresetsDirectories.ContainsByPredicate(
				[Path, InSection](const FDirectoryPath& DirectoryPath)
				{
					return DirectoryPath.Path == Path;
				});

		if (!bAlreadyContinsPath)
		{
			FProperty* Property = UMetaHumanCharacterEditorSettings::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, PresetsDirectories));
			MetaHumanEditorSettings->PreEditChange(Property);

			MetaHumanEditorSettings->PresetsDirectories.Add(FDirectoryPath(Path));

			FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
			MetaHumanEditorSettings->PostEditChangeProperty(PropertyChangedEvent);
		}
	}
}

void SMetaHumanCharacterEditorBlendToolPanel::OnBlendToolVirtualItemDeleted(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	UMetaHumanCharacter* Character = CharacterWeakPtr.IsValid() ? CharacterWeakPtr.Get() : nullptr;
	if (!Character || !Item.IsValid())
	{
		return;
	}

	UMetaHumanCharacter* CharacterItem = Cast<UMetaHumanCharacter>(Item->AssetData.GetAsset());
	FMetaHumanCharacterIndividualAssets* IndividualAssets = Character->CharacterIndividualAssets.Find(VirtualFolderSlotName);
	if (!CharacterItem || !IndividualAssets)
	{
		return;
	}

	if (IndividualAssets->Characters.Contains(CharacterItem))
	{
		Character->Modify();
		IndividualAssets->Characters.Remove(TNotNull<UMetaHumanCharacter*>(CharacterItem));
	}
}

bool SMetaHumanCharacterEditorBlendToolPanel::CanDeleteBlendToolVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const
{
	UMetaHumanCharacter* Character = CharacterWeakPtr.IsValid() ? CharacterWeakPtr.Get() : nullptr;
	if (!Character || !Item.IsValid() || !Item->AssetData.IsAssetLoaded())
	{
		return false;
	}

	UMetaHumanCharacter* CharacterItem = Cast<UMetaHumanCharacter>(Item->AssetData.GetAsset());
	FMetaHumanCharacterIndividualAssets* IndividualAssets = Character->CharacterIndividualAssets.Find(VirtualFolderSlotName);
	if (!CharacterItem || !IndividualAssets)
	{
		return false;
	}

	return IndividualAssets->Characters.Contains(CharacterItem);
}

void SMetaHumanCharacterEditorBlendToolPanel::OnPresetsPathsFolderDeleted(const FMetaHumanCharacterAssetsSection& InSection)
{
	UMetaHumanCharacterEditorSettings* MetaHumanEditorSettings = GetMutableDefault<UMetaHumanCharacterEditorSettings>();
	if (!MetaHumanEditorSettings)
	{
		return;
	}

	FProperty* Property = UMetaHumanCharacterEditorSettings::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, WardrobePaths));
	MetaHumanEditorSettings->PreEditChange(Property);

	MetaHumanEditorSettings->WardrobePaths.SetNum(Algo::RemoveIf(MetaHumanEditorSettings->PresetsDirectories,
		[InSection](const FDirectoryPath& DirectoryPath)
		{
			return DirectoryPath.Path == InSection.ContentDirectoryToMonitor.Path;
		}));

	FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
	MetaHumanEditorSettings->PostEditChangeProperty(PropertyChangedEvent);
}

bool SMetaHumanCharacterEditorBlendToolPanel::CanDeletePresetsPathsFolder(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item, const FMetaHumanCharacterAssetsSection& InSection) const
{
	UMetaHumanCharacterEditorSettings* MetaHumanEditorSettings = GetMutableDefault<UMetaHumanCharacterEditorSettings>();
	if (!MetaHumanEditorSettings)
	{
		return false;
	}

	return Algo::FindByPredicate(MetaHumanEditorSettings->PresetsDirectories,
		[InSection](const FDirectoryPath& DirectoryPath)
		{
			return DirectoryPath.Path == InSection.ContentDirectoryToMonitor.Path;
		}) != nullptr;
}

void SMetaHumanCharacterEditorBlendToolPanel::OnHandleBlendVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	UMetaHumanCharacter* Character = CharacterWeakPtr.IsValid() ? CharacterWeakPtr.Get() : nullptr;
	if (!Character || !Item.IsValid())
	{
		return;
	}

	UMetaHumanCharacter* CharacterItem = Cast<UMetaHumanCharacter>(Item->AssetData.GetAsset());
	if (!CharacterItem)
	{
		return;
	}

	FMetaHumanCharacterIndividualAssets& IndividualAssets = Character->CharacterIndividualAssets.FindOrAdd(VirtualFolderSlotName);
	if (!IndividualAssets.Characters.Contains(CharacterItem))
	{
		Character->Modify();
		IndividualAssets.Characters.Add(TNotNull<UMetaHumanCharacter*>(CharacterItem));
	}
}

void SMetaHumanCharacterEditorBlendToolPanel::OnPresetsDirectoriesChanged()
{
	if (AssetViewsPanel.IsValid())
	{
		AssetViewsPanel->RequestRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
