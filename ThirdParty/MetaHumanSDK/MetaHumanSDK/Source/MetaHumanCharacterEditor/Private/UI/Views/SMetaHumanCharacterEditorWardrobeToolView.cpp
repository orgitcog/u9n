// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorWardrobeToolView.h"

#include "Algo/Find.h"
#include "Algo/RemoveIf.h"
#include "Algo/Transform.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/UObjectToken.h"
#include "AssetToolsModule.h"
#include "AssetThumbnail.h"
#include "ClassIconFinder.h"
#include "ContentBrowserItem.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/SToolBarButtonBlock.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "IDetailsView.h"
#include "ImageCoreUtils.h"
#include "ImageUtils.h"
#include "InteractiveToolManager.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Logging/StructuredLog.h"
#include "MetaHumanCharacterAnalytics.h"
#include "MetaHumanCharacterAssetObserver.h"
#include "MetaHumanCharacterEditorCommands.h"
#include "MetaHumanCharacterEditorLog.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "MetaHumanCharacterEditorSettings.h"
#include "MetaHumanCharacterEditorWardrobeSettings.h"
#include "MetaHumanCharacterEditorModule.h"
#include "MetaHumanCharacterPaletteEditorModule.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanCollectionPipeline.h"
#include "MetaHumanWardrobeItem.h"
#include "MetaHumanWardrobeItemFactory.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PropertyEditorModule.h"
#include "Slate/DeferredCleanupSlateBrush.h"
#include "Tools/MetaHumanCharacterEditorWardrobeTools.h"
#include "UI/Widgets/SMetaHumanCharacterEditorAssetViewsPanel.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "SWarningOrErrorBox.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor/EditorEngine.h"
#include "Logging/MessageLog.h"
#include "GroomBindingAsset.h"
#include "GroomAsset.h"
#include "ProjectUtilities/MetaHumanAssetManager.h"
#include "Animation/Skeleton.h"

extern UNREALED_API UEditorEngine* GEditor;

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorWardrobeToolView"

void SMetaHumanCharacterEditorWardrobeToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorWardrobeTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorWardrobeToolView::GetToolProperties() const
{
	const UMetaHumanCharacterEditorWardrobeTool* WardrobeTool = Cast<UMetaHumanCharacterEditorWardrobeTool>(Tool);
	return IsValid(WardrobeTool) ? WardrobeTool->GetWardrobeToolProperties() : nullptr;
}

void SMetaHumanCharacterEditorWardrobeToolView::MakeToolView()
{
	if(ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateWardrobeToolViewAssetViewsPanelSection()
				]
			];
	}

	if (ToolViewMainBox.IsValid())
	{
		ToolViewMainBox->AddSlot()
			.Padding(0.f, 4.f, 0.f, 0.f)
			.AutoHeight()
			[
				CreateWardrobeToolViewToolbarSection()
			];
	}
}

void SMetaHumanCharacterEditorWardrobeToolView::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	OnPreEditChangeProperty(PropertyAboutToChange, PropertyAboutToChange->GetName());
}

void SMetaHumanCharacterEditorWardrobeToolView::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	const bool bIsInteractive = PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive;
	OnPostEditChangeProperty(PropertyThatChanged, bIsInteractive);
}

TSharedRef<SWidget> SMetaHumanCharacterEditorWardrobeToolView::CreateWardrobeToolViewAssetViewsPanelSection()
{
	UMetaHumanCharacterEditorWardrobeTool* WardrobeTool = Cast<UMetaHumanCharacterEditorWardrobeTool>(Tool);
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	const UMetaHumanCharacter* Character = WardrobeTool ? UE::ToolTarget::GetTargetMetaHumanCharacter(WardrobeTool->GetTarget()) : nullptr;
	if (!IsValid(Character) || !WardrobeToolProperties)
	{
		return SNullWidget::NullWidget;
	}

	UMetaHumanCharacterEditorSettings* MetaHumanEditorSettings = GetMutableDefault<UMetaHumanCharacterEditorSettings>();
	if (!MetaHumanEditorSettings->OnWardrobePathsChanged.IsBoundToObject(this))
	{
		MetaHumanEditorSettings->OnWardrobePathsChanged.AddSP(this, &SMetaHumanCharacterEditorWardrobeToolView::OnWardrobePathsChanged);
	}

	const UMetaHumanCharacterPipelineSpecification* Specification = WardrobeToolProperties->Collection->GetPipeline()->GetSpecification();
	const TArray<FMetaHumanCharacterAssetsSection> Sections = GetWardrobeAssetViewsSections(Character, Specification);
	const TSharedRef<SWidget> AssetsViewSectionWidget =
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(4.f)
		.AutoHeight()
		[
			SAssignNew(AssetViewsPanel, SMetaHumanCharacterEditorAssetViewsPanel)
			.AssetViewSections(this, &SMetaHumanCharacterEditorWardrobeToolView::GetWardrobeAssetViewsSections, Character, Specification)
			.VirtualFolderClassesToFilter({ UMetaHumanWardrobeItem::StaticClass() })
			.AllowDragging(false)
			.AllowSlots(true)
			.AllowMultiSelection(true)
			.AllowSlotMultiSelection(true)
			.IsItemCompatible(this, &SMetaHumanCharacterEditorWardrobeToolView::IsItemCompatible)
			.IsItemChecked(this, &SMetaHumanCharacterEditorWardrobeToolView::IsItemChecked)
			.IsItemActive(this, &SMetaHumanCharacterEditorWardrobeToolView::IsItemActive)
			.OnOverrideSlotName(this, &SMetaHumanCharacterEditorWardrobeToolView::OnOverrideSlotName)
			.OnOverrideThumbnail(this, &SMetaHumanCharacterEditorWardrobeToolView::OnOverrideItemThumbnailBrush)
			.OnOverrideThumbnailName(this, &SMetaHumanCharacterEditorWardrobeToolView::OnOverrideItemThumbnailName)
			.OnProcessDroppedItem(this, &SMetaHumanCharacterEditorWardrobeToolView::OnProcessDroppedItem)
			.OnProcessDroppedFolders(this, &SMetaHumanCharacterEditorWardrobeToolView::OnProcessDroppedFolders)
			.OnPopulateAssetViewsItems(this, &SMetaHumanCharacterEditorWardrobeToolView::OnPopulateAssetViewsItems)
			.OnItemActivated(this, &SMetaHumanCharacterEditorWardrobeToolView::OnWardrobeToolItemActivated)
			.OnItemDeleted(this, &SMetaHumanCharacterEditorWardrobeToolView::OnWardrobeToolVirtualItemDeleted)
			.CanDeleteItem(this, &SMetaHumanCharacterEditorWardrobeToolView::CanDeleteWardrobeToolVirtualItem)
			.OnFolderDeleted(this, &SMetaHumanCharacterEditorWardrobeToolView::OnWardrobePathsFolderDeleted)
			.CanDeleteFolder(this, &SMetaHumanCharacterEditorWardrobeToolView::CanDeleteWardrobePathsFolder)
			.OnHadleVirtualItem(this, &SMetaHumanCharacterEditorWardrobeToolView::OnHandleWardrobeVirtualItem)
		];

	return AssetsViewSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorWardrobeToolView::CreateWardrobeToolViewToolbarSection()
{
	RegisterToolbarCommands();

	const TSharedRef<FSlimHorizontalUniformToolBarBuilder> ToolbarBuilder = MakeShareable(new FSlimHorizontalUniformToolBarBuilder(CommandList, FMultiBoxCustomization(TEXT("SlimHorizontal"))));
	ToolbarBuilder->SetStyle(&FAppStyle::Get(), "SlimPaletteToolBar");

	const FMetaHumanCharacterEditorToolCommands& Commands = FMetaHumanCharacterEditorToolCommands::Get();
	FButtonArgs PrepareAccessoryArgs;
	PrepareAccessoryArgs.CommandList = CommandList;
	PrepareAccessoryArgs.Command = Commands.PrepareAccessory;
	ToolbarBuilder->AddToolBarButton(PrepareAccessoryArgs);

	FButtonArgs UnprepareAccessoryArgs;
	UnprepareAccessoryArgs.CommandList = CommandList;
	UnprepareAccessoryArgs.Command = Commands.UnprepareAccessory;
	ToolbarBuilder->AddToolBarButton(UnprepareAccessoryArgs);

	FButtonArgs WearAccessoryArgs;
	WearAccessoryArgs.CommandList = CommandList;
	WearAccessoryArgs.Command = Commands.WearAcceessory;
	ToolbarBuilder->AddToolBarButton(WearAccessoryArgs);

	FButtonArgs RemoveAccessoryArgs;
	RemoveAccessoryArgs.CommandList = CommandList;
	RemoveAccessoryArgs.Command = Commands.RemoveAccessory;
	ToolbarBuilder->AddToolBarButton(RemoveAccessoryArgs);

	FButtonArgs AccessoryPropertiesArgs;
	AccessoryPropertiesArgs.CommandList = CommandList;
	AccessoryPropertiesArgs.Command = Commands.AccessoryProperties;
	ToolbarBuilder->AddToolBarButton(AccessoryPropertiesArgs);

	const TSharedRef<SWidget> ToolbarSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("WardrobeToolbarSectionLabel", "Accessory"))
		.Padding(-4.f)
		.RoundedBorders(false)
		.Content()
		[
			SNew(SVerticalBox)

			// Toolbar buttons section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(4.f, 4.f, 4.f, 8.f)
			.AutoHeight()
			[
				ToolbarBuilder->MakeWidget()
			]
		];

	return ToolbarSectionWidget;
}

void SMetaHumanCharacterEditorWardrobeToolView::CreateWardrobeItemsForCompatibleAssets(const FMetaHumanCharacterAssetsSection& Section)
{
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;
	if (!IsValid(Collection))
	{
		return;
	}

	FString LocalFolderPath;
	if (!FPackageName::TryConvertGameRelativePackagePathToLocalPath(Section.ContentDirectoryToMonitor.Path, LocalFolderPath))
	{
		return;
	}

	if (!IFileManager::Get().DirectoryExists(*LocalFolderPath))
	{
		return;
	}

	const FMetaHumanCharacterPipelineSlot* PipelineSlot = Collection->GetPipeline()->GetSpecification()->Slots.Find(Section.SlotName);
	if (!PipelineSlot)
	{
		return;
	}

	TArray<FAssetData> FoundWardrobeItemAssets;
	FMetaHumanCharacterAssetObserver::Get().GetWardrobeAssets(
		FName(Section.ContentDirectoryToMonitor.Path),
		TSet(Section.ClassesToFilter),
		FoundWardrobeItemAssets);

	UMetaHumanWardrobeItemFactory* WardrobeItemFactory = NewObject<UMetaHumanWardrobeItemFactory>();
	for (const TSoftClassPtr<UObject>& SupportedType : PipelineSlot->SupportedPrincipalAssetTypes)
	{
		if (!SupportedType.IsValid())
		{
			continue;
		}

		TArray<FAssetData> FoundAssets;
		FMetaHumanCharacterAssetObserver::Get().GetAssets(
			FName(Section.ContentDirectoryToMonitor.Path),
			{ SupportedType.Get() },
			FoundAssets);

		for (const FAssetData& AssetData : FoundAssets)
		{
			if (!AssetData.IsValid())
			{
				continue;
			}

			const bool bItemAlreadyExists = FoundWardrobeItemAssets.ContainsByPredicate(
				[AssetData](const FAssetData& ItemData)
				{
					const UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(ItemData.GetAsset());
					if (WardrobeItem)
					{
						return WardrobeItem->PrincipalAsset.ToSoftObjectPath() == AssetData.ToSoftObjectPath();
					}

					return false;
				});

			if (bItemAlreadyExists)
			{
				continue;
			}

			FString NewName = FString::Printf(TEXT("WI_%s"), *AssetData.AssetName.ToString());

			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			UObject* NewWardrobeItemObject = AssetTools.CreateAsset(NewName, Section.ContentDirectoryToMonitor.Path, UMetaHumanWardrobeItem::StaticClass(), WardrobeItemFactory);
			if (UMetaHumanWardrobeItem* NewWardrobeItem = Cast<UMetaHumanWardrobeItem>(NewWardrobeItemObject))
			{
				NewWardrobeItem->PrincipalAsset = AssetData.GetSoftObjectPath();
			}
		}
	}
}

void SMetaHumanCharacterEditorWardrobeToolView::RegisterToolbarCommands()
{
	const FMetaHumanCharacterEditorToolCommands& Commands = FMetaHumanCharacterEditorToolCommands::Get();
	CommandList = MakeShareable(new FUICommandList);

	CommandList->MapAction
	(
		Commands.PrepareAccessory,
		FExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorWardrobeToolView::OnPrepareAccessory)
	);

	CommandList->MapAction
	(
		Commands.UnprepareAccessory,
		FExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorWardrobeToolView::OnUnprepareAccessory)
	);

	CommandList->MapAction
	(
		Commands.WearAcceessory,
		FExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorWardrobeToolView::OnWearAccessory)
	);

	CommandList->MapAction
	(
		Commands.RemoveAccessory,
		FExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorWardrobeToolView::OnRemoveAccessory)
	);

	CommandList->MapAction
	(
		Commands.AccessoryProperties,
		FExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorWardrobeToolView::OnOpenAccessoryProperties)
	);
}

TArray<FMetaHumanCharacterAssetViewItem> SMetaHumanCharacterEditorWardrobeToolView::GetWardrobeIndividualAssets(const FName& SlotName) const
{
	TArray<FMetaHumanCharacterAssetViewItem> Items;

	UMetaHumanCharacterEditorWardrobeTool* WardrobeTool = Cast<UMetaHumanCharacterEditorWardrobeTool>(Tool);
	const UMetaHumanCharacter* Character = WardrobeTool ? UE::ToolTarget::GetTargetMetaHumanCharacter(WardrobeTool->GetTarget()) : nullptr;
	if (!Character)
	{
		return Items;
	}

	const FMetaHumanCharacterWardrobeIndividualAssets* IndividualAssets = Character->WardrobeIndividualAssets.Find(SlotName);
	if (!IndividualAssets)
	{
		return Items;
	}

	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;
	if (!Collection)
	{
		return Items;
	}

	const TArray<FMetaHumanCharacterPaletteItem>& PaletteItems = Collection->GetItems();

	for (TSoftObjectPtr<UMetaHumanWardrobeItem> Item : IndividualAssets->Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		const FAssetData AssetData = FAssetData(Item.Get());
		FMetaHumanPaletteItemKey PaletteItemKey;

		const FMetaHumanCharacterPaletteItem* FoundItem = PaletteItems.FindByPredicate(
			[&AssetData, SlotName](const FMetaHumanCharacterPaletteItem& Item)
			{
				return
					Item.SlotName == SlotName &&
					Item.WardrobeItem &&
					Item.WardrobeItem->IsExternal() &&
					FSoftObjectPath(Item.WardrobeItem) == AssetData.ToSoftObjectPath();
			});

		if (FoundItem)
		{
			PaletteItemKey = FoundItem->GetItemKey();
		}

		const FMetaHumanCharacterAssetViewItem AssetItem(AssetData, SlotName, PaletteItemKey, nullptr);
		Items.Add(AssetItem);
	}

	// Sort assets by name
	Items.Sort([](const FMetaHumanCharacterAssetViewItem& ItemA, const FMetaHumanCharacterAssetViewItem& ItemB)
		{
			return ItemA.AssetData.AssetName.Compare(ItemB.AssetData.AssetName) < 0;
		});
	
	return Items;
}

TArray<FMetaHumanCharacterAssetsSection> SMetaHumanCharacterEditorWardrobeToolView::GetWardrobeAssetViewsSections(const UMetaHumanCharacter* InCharacter, const UMetaHumanCharacterPipelineSpecification* InSpec) const
{
	// TODO: Sort this by targeted filter, if possible.
	TArray<FMetaHumanCharacterAssetsSection> Sections;

	if (FMetaHumanCharacterEditorModule::IsOptionalMetaHumanContentInstalled())
	{
		const UMetaHumanCharacterEditorWardrobeSettings* WardrobeSettings = GetDefault<UMetaHumanCharacterEditorWardrobeSettings>();

		// Add editor predefined paths
		for (const FMetaHumanCharacterAssetsSection& InSection : WardrobeSettings->WardrobeSections)
		{
			Sections.AddUnique(InSection);
		}
	}

	// Add sections from the character
	for (const FMetaHumanCharacterAssetsSection& InSection : InCharacter->WardrobePaths)
	{
		Sections.AddUnique(InSection);
	}

	// Append user sections from project settings
	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();
	for (const FMetaHumanCharacterAssetsSection& InSection : Settings->WardrobePaths)
	{
		Sections.AddUnique(InSection);
	}

	// Filter valid section settings
	TArray<FName> SpecificationSlotNames;
	InSpec->Slots.GetKeys(SpecificationSlotNames);
	return Sections.FilterByPredicate([InSpec, SpecificationSlotNames](const FMetaHumanCharacterAssetsSection& Section)
		{
			if (Section.ClassesToFilter.IsEmpty())
			{
				return false;
			}

			FString LongPackageName;
			// Check if the section is pure virtual or if we provided the long package name
			if (!Section.bPureVirtual && 
				!FPackageName::TryConvertLongPackageNameToFilename(
				Section.ContentDirectoryToMonitor.Path,
				LongPackageName))
			{
				return false;
			}

			if (Section.SlotName == NAME_None)
			{
				return true;
			}

			const FMetaHumanCharacterPipelineSlot* SlotSpec = InSpec->Slots.Find(Section.SlotName);
			if (!SlotSpec)
			{
				return false;
			}

			if (!SpecificationSlotNames.Contains(Section.SlotName))
			{
				return false;
			}

			// Check if filter classes are supported
			for (const TSubclassOf<UObject>& FilterClass : Section.ClassesToFilter)
			{
				if (!FilterClass || !SlotSpec->SupportsAssetType(FilterClass))
				{
					return false;
				}
			}

			return true;
		});
}

bool SMetaHumanCharacterEditorWardrobeToolView::IsItemCompatible(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item, const FMetaHumanCharacterAssetsSection& Section) const
{
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;
	const UObject* AssetObject = Item.IsValid() ? Item->AssetData.GetAsset() : nullptr;
	if (!IsValid(Collection) || !IsValid(AssetObject))
	{
		return false;
	}

	const UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(AssetObject);
	if (!IsValid(WardrobeItem))
	{
		return false;
	}

	if (const FMetaHumanCharacterPipelineSlot* PipelineSlot = Collection->GetPipeline()->GetSpecification()->Slots.Find(Item->SlotName))
	{
		const FAssetData PrincipalAssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(WardrobeItem->PrincipalAsset.ToSoftObjectPath());
		if (PrincipalAssetData.IsValid())
		{
			return PipelineSlot->SupportsAsset(PrincipalAssetData);
		}
	}
	
	if (Item->SlotName == NAME_None)
	{
		return true;
	}

	return false;
}

bool SMetaHumanCharacterEditorWardrobeToolView::IsItemChecked(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const
{
	return Item.IsValid() && !Item->PaletteItemKey.IsNull();
}

bool SMetaHumanCharacterEditorWardrobeToolView::IsItemActive(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const
{
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;
	if (!Collection || !Item.IsValid())
	{
		return false;
	}

	const FMetaHumanPipelineSlotSelection Selection(Item->SlotName, Item->PaletteItemKey);
	return Collection->GetMutableDefaultInstance()->ContainsSlotSelection(Selection);
}

FName SMetaHumanCharacterEditorWardrobeToolView::OnOverrideSlotName(const FName& SlotName)
{
	const UMetaHumanCharacterEditorWardrobeSettings* WardrobeSettings = GetDefault<UMetaHumanCharacterEditorWardrobeSettings>();
	if (WardrobeSettings)
	{
		return *WardrobeSettings->SlotNameToCategoryName(SlotName, FText::FromName(SlotName)).ToString();
	}

	return SlotName;
}

FText SMetaHumanCharacterEditorWardrobeToolView::OnOverrideItemThumbnailName(TSharedPtr<FMetaHumanCharacterAssetViewItem> InItem) const
{
	if (InItem.IsValid())
	{
		if (UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(InItem->AssetData.GetAsset()))
		{
			return WardrobeItem->ThumbnailName;
		}
	}
	
	return FText::GetEmpty();
}

void SMetaHumanCharacterEditorWardrobeToolView::OnOverrideItemThumbnailBrush(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const
{
	if (!Item.IsValid())
	{
		return;
	}

	// We need to load wardrobe item here to access the principal asset
	UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(Item->AssetData.GetAsset());
	if (!WardrobeItem)
	{
		return;
	}
	
	if (WardrobeItem->ThumbnailImage.ToSoftObjectPath().IsValid())
	{
		UTexture2D* Tex = WardrobeItem->ThumbnailImage.LoadSynchronous();
		Item->ThumbnailImageOverride = FDeferredCleanupSlateBrush::CreateBrush(Tex);
		return;
	}
	
	auto GetThumbnailMapFromPackage = [](const FAssetData& AssetData, FThumbnailMap& OutThumbnailMap)
	{
		OutThumbnailMap.Empty();
		FString PackageFilename;
		if (FPackageName::DoesPackageExist(AssetData.PackageName.ToString(), &PackageFilename))
		{
			const FName ObjectFullName = FName(*AssetData.GetFullName());
			TSet<FName> ObjectFullNames;
			ObjectFullNames.Add(ObjectFullName);

			ThumbnailTools::LoadThumbnailsFromPackage(PackageFilename, ObjectFullNames, OutThumbnailMap);
		}
	};

	FAssetData PrincipalAssetData;

	// Principal asset is required for the thumbnail override
	if (IAssetRegistry::GetChecked().TryGetAssetByObjectPath(WardrobeItem->PrincipalAsset.ToSoftObjectPath(), PrincipalAssetData) != UE::AssetRegistry::EExists::Exists)
	{
		return;
	}

	// Needs to be in this scope as we're reading data from it through pointers
	FThumbnailMap ThumbnailMap;

	// TODO: In future, we might want to load thumbnails from the Wardrobe Item first as they could
	// contain custom thumbnail (e.g. better camera angles), but for now we just use the principal
	// item asset.

	// Read the thumbnail from cache
	const FObjectThumbnail* AssetThumbnail = ThumbnailTools::FindCachedThumbnail(PrincipalAssetData.GetFullName());

	// If cache is empty, load from the principal asset package
	if (!(AssetThumbnail && !AssetThumbnail->IsEmpty()))
	{
		GetThumbnailMapFromPackage(PrincipalAssetData, ThumbnailMap);

		if (FObjectThumbnail* FoundThumbnail = ThumbnailMap.Find(*PrincipalAssetData.GetFullName()))
		{
			AssetThumbnail = FoundThumbnail;
		}
	}

	// Create texture from the found thumbnail
	if (AssetThumbnail && !AssetThumbnail->IsEmpty())
	{
		const TArray<uint8>& ImageData = AssetThumbnail->GetUncompressedImageData();
		if (ImageData.Num() > 0)
		{
			UTexture2D* Texture = UTexture2D::CreateTransient(
				AssetThumbnail->GetImageWidth(),
				AssetThumbnail->GetImageHeight(),
				FImageCoreUtils::GetPixelFormatForRawImageFormat(AssetThumbnail->GetImage().Format),
				NAME_None,
				ImageData);

			if (Texture)
			{
				Item->ThumbnailImageOverride = FDeferredCleanupSlateBrush::CreateBrush(Texture);
				return;
			}
		}
	}
	
	// Couldn't load thumbnail for the principal asset, fallback to using the class thumbnail
	const UClass* AssetClass = FClassIconFinder::GetIconClassForAssetData(PrincipalAssetData);
	const FSlateBrush* ClassThumbBrush = FClassIconFinder::FindThumbnailForClass(AssetClass);
	Item->ThumbnailImageOverride = FDeferredCleanupSlateBrush::CreateBrush(*ClassThumbBrush);
}

UObject* SMetaHumanCharacterEditorWardrobeToolView::OnProcessDroppedItem(const FAssetData& AssetData)
{
	UObject* AssetObject = AssetData.GetAsset();
	if (!AssetObject || Cast<UMetaHumanWardrobeItem>(AssetObject))
	{
		return AssetObject;
	}

	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	
	for (const TPair<FName, FMetaHumanCharacterPipelineSlot>& SlotIt : WardrobeToolProperties->Collection->GetPipeline()->GetSpecification()->Slots)
	{
		if (SlotIt.Value.SupportsAssetType(AssetObject->GetClass()))
		{
			const FString ObjectName = TEXT("WI_") + AssetObject->GetName();
			UMetaHumanWardrobeItem* NewWardrobeItem = NewObject<UMetaHumanWardrobeItem>(GetTransientPackage(), *ObjectName);
			NewWardrobeItem->PrincipalAsset = AssetObject;

			return NewWardrobeItem;
		}
	}

	return nullptr;
}

void SMetaHumanCharacterEditorWardrobeToolView::OnProcessDroppedFolders(const TArray<FContentBrowserItem> Items, const FMetaHumanCharacterAssetsSection& InSection) const
{
	UMetaHumanCharacterEditorSettings* MetaHumanEditorSettings = GetMutableDefault<UMetaHumanCharacterEditorSettings>();
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;
	if (!Collection || !MetaHumanEditorSettings || Items.IsEmpty())
	{
		return;
	}

	const FMetaHumanCharacterPipelineSlot* PipelineSlot = Collection->GetPipeline()->GetSpecification()->Slots.Find(InSection.SlotName);
	if (!PipelineSlot)
	{
		return;
	}
	
	const TArray<TSoftClassPtr<UObject>> SupportedTypes = PipelineSlot->SupportedPrincipalAssetTypes;
	TArray<TSubclassOf<UObject>> SupportedClasses;
	Algo::TransformIf(SupportedTypes, SupportedClasses,
		[](const TSoftClassPtr<UObject>& SoftClassPtr)
		{
			return SoftClassPtr.IsValid();
		},
		[](const TSoftClassPtr<UObject>& SoftClassPtr)
		{
			return SoftClassPtr.Get();
		});

	for (const FContentBrowserItem& Item : Items)
	{
		if (!Item.IsFolder())
		{
			continue;
		}

		const FString Path = Item.GetInternalPath().ToString();
		const bool bAlreadyContinsPath =
			MetaHumanEditorSettings->WardrobePaths.ContainsByPredicate(
				[Path, InSection](const FMetaHumanCharacterAssetsSection& Section)
				{
					return 
						Section.ContentDirectoryToMonitor.Path == Path &&
						Section.SlotName == InSection.SlotName;
				});

		if (!bAlreadyContinsPath)
		{
			FMetaHumanCharacterAssetsSection NewSection;
			NewSection.ContentDirectoryToMonitor = FDirectoryPath(Path);
			NewSection.ClassesToFilter = SupportedClasses;
			NewSection.SlotName = InSection.SlotName;

			FProperty* Property = UMetaHumanCharacterEditorSettings::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, WardrobePaths));
			MetaHumanEditorSettings->PreEditChange(Property);

			MetaHumanEditorSettings->WardrobePaths.AddUnique(NewSection);

			FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
			MetaHumanEditorSettings->PostEditChangeProperty(PropertyChangedEvent);
		}
	}
}

TArray<FMetaHumanCharacterAssetViewItem> SMetaHumanCharacterEditorWardrobeToolView::OnPopulateAssetViewsItems(const FMetaHumanCharacterAssetsSection& InSection, const FMetaHumanObserverChanges& InChanges)
{
	TArray<FMetaHumanCharacterAssetViewItem> Items;
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;
	if (!IsValid(Collection))
	{
		return Items;
	}

	const TArray<FMetaHumanCharacterPaletteItem>& PaletteItems = Collection->GetItems();
	const FName SlotName = InSection.SlotName;

	// Remove items from palette that were just deleted
	if (const TArray<TSoftObjectPtr<UObject>>* DeletedAssets = InChanges.Changes.Find(FMetaHumanObserverChanges::EChangeType::Removed))
	{
		for (int32 Index = 0; Index < DeletedAssets->Num(); Index++)
		{
			const TSoftObjectPtr<UObject>& Asset = DeletedAssets->operator[](Index);

			// Remove all items that reference this principal asset
			for (int32 ItemIndex = PaletteItems.Num() - 1; ItemIndex >= 0; ItemIndex--)
			{
				const FMetaHumanCharacterPaletteItem& Item = PaletteItems[ItemIndex];

				if (Item.SlotName == InSection.SlotName && 
					Item.WardrobeItem && 
					Item.WardrobeItem->PrincipalAsset == Asset)
				{
					ensure(Collection->TryRemoveItem(Item.GetItemKey()));
				}
			}
		}
	}

	const FMetaHumanCharacterPipelineSlot* Slot = Collection->GetPipeline()->GetSpecification()->Slots.Find(SlotName);
	// Slot not supported
	if (!Slot && SlotName != NAME_None)
	{
		return Items;
	}

	if (InSection.bPureVirtual)
	{
		Items.Append(GetWardrobeIndividualAssets(SlotName));
		return Items;
	}

	CreateWardrobeItemsForCompatibleAssets(InSection);
	TArray<FAssetData> FoundAssets;
	if (SlotName == NAME_None)
	{
		FMetaHumanCharacterAssetObserver::Get().GetAssets(
			FName(InSection.ContentDirectoryToMonitor.Path),
			TSet(InSection.ClassesToFilter),
			FoundAssets);
	}
	else
	{
		FMetaHumanCharacterAssetObserver::Get().GetWardrobeAssets(
			FName(InSection.ContentDirectoryToMonitor.Path),
			TSet(InSection.ClassesToFilter),
			FoundAssets);
	}

	// Sort assets by name
	FoundAssets.Sort([](const FAssetData& AssetA, const FAssetData& AssetB)
		{
			return AssetA.AssetName.Compare(AssetB.AssetName) < 0;
		});

	for (const FAssetData& Asset : FoundAssets)
	{
		FMetaHumanPaletteItemKey PaletteItemKey;

		const FMetaHumanCharacterPaletteItem* FoundItem = PaletteItems.FindByPredicate(
			[&Asset, SlotName](const FMetaHumanCharacterPaletteItem& Item)
			{
				return 
					Item.SlotName == SlotName && 
					Item.WardrobeItem && 
					Item.WardrobeItem->IsExternal()	&& 
					FSoftObjectPath(Item.WardrobeItem) == Asset.ToSoftObjectPath();
			});

		if (FoundItem)
		{
			PaletteItemKey = FoundItem->GetItemKey();
		}

		Items.Add(FMetaHumanCharacterAssetViewItem(Asset, InSection.SlotName, PaletteItemKey, nullptr));
	}

	return Items;
}

void SMetaHumanCharacterEditorWardrobeToolView::OnWardrobeToolItemActivated(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	if (AssetViewsPanel.IsValid())
	{
		if (ApplyWearRequest({ Item }, EWearRequest::Toggle))
		{
			AssetViewsPanel->RequestRefresh();
		}
	}
}

void SMetaHumanCharacterEditorWardrobeToolView::OnWardrobeToolVirtualItemDeleted(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	UMetaHumanCharacterEditorWardrobeTool* WardrobeTool = Cast<UMetaHumanCharacterEditorWardrobeTool>(Tool);
	UMetaHumanCharacter* Character = WardrobeTool ? UE::ToolTarget::GetTargetMetaHumanCharacter(WardrobeTool->GetTarget()) : nullptr;
	if (!Character || !Item.IsValid())
	{
		return;
	}

	UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(Item->AssetData.GetAsset());
	FMetaHumanCharacterWardrobeIndividualAssets* IndividualAssets = Character->WardrobeIndividualAssets.Find(Item->SlotName);
	if (!WardrobeItem || !IndividualAssets)
	{
		return;
	}

	if (IndividualAssets->Items.Contains(WardrobeItem))
	{
		Character->Modify();
		IndividualAssets->Items.Remove(TNotNull<UMetaHumanWardrobeItem*>(WardrobeItem));
	}
}

bool SMetaHumanCharacterEditorWardrobeToolView::CanDeleteWardrobeToolVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const
{
	UMetaHumanCharacterEditorWardrobeTool* WardrobeTool = Cast<UMetaHumanCharacterEditorWardrobeTool>(Tool);
	UMetaHumanCharacter* Character = WardrobeTool ? UE::ToolTarget::GetTargetMetaHumanCharacter(WardrobeTool->GetTarget()) : nullptr;
	if (!Character || !Item.IsValid() || !Item->AssetData.IsAssetLoaded())
	{
		return false;
	}

	UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(Item->AssetData.GetAsset());
	FMetaHumanCharacterWardrobeIndividualAssets* IndividualAssets = Character->WardrobeIndividualAssets.Find(Item->SlotName);
	if (!WardrobeItem || !IndividualAssets)
	{
		return false;
	}

	return IndividualAssets->Items.Contains(WardrobeItem);
}

void SMetaHumanCharacterEditorWardrobeToolView::OnWardrobePathsFolderDeleted(const FMetaHumanCharacterAssetsSection& InSection)
{
	UMetaHumanCharacterEditorSettings* MetaHumanEditorSettings = GetMutableDefault<UMetaHumanCharacterEditorSettings>();
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;
	if (!Collection || !MetaHumanEditorSettings)
	{
		return;
	}

	const FMetaHumanCharacterPipelineSlot* PipelineSlot = Collection->GetPipeline()->GetSpecification()->Slots.Find(InSection.SlotName);
	if (!PipelineSlot)
	{
		return;
	}

	const TArray<TSoftClassPtr<UObject>> SupportedTypes = PipelineSlot->SupportedPrincipalAssetTypes;
	TArray<TSubclassOf<UObject>> SupportedClasses;
	Algo::TransformIf(SupportedTypes, SupportedClasses,
		[](const TSoftClassPtr<UObject>& SoftClassPtr)
		{
			return SoftClassPtr.IsValid();
		},
		[](const TSoftClassPtr<UObject>& SoftClassPtr)
		{
			return SoftClassPtr.Get();
		});

	FProperty* Property = UMetaHumanCharacterEditorSettings::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, WardrobePaths));
	MetaHumanEditorSettings->PreEditChange(Property);

	MetaHumanEditorSettings->WardrobePaths.SetNum(Algo::RemoveIf(MetaHumanEditorSettings->WardrobePaths,
			[InSection, SupportedClasses](const FMetaHumanCharacterAssetsSection& Section)
			{
				return
					Section.ContentDirectoryToMonitor.Path == InSection.ContentDirectoryToMonitor.Path &&
					Section.ClassesToFilter == SupportedClasses &&
					Section.SlotName == InSection.SlotName;
			}));

	FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
	MetaHumanEditorSettings->PostEditChangeProperty(PropertyChangedEvent);
}

bool SMetaHumanCharacterEditorWardrobeToolView::CanDeleteWardrobePathsFolder(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item, const FMetaHumanCharacterAssetsSection& InSection) const
{
	UMetaHumanCharacterEditorSettings* MetaHumanEditorSettings = GetMutableDefault<UMetaHumanCharacterEditorSettings>();
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;
	if (!Collection || !MetaHumanEditorSettings)
	{
		return false;
	}

	const FMetaHumanCharacterPipelineSlot* PipelineSlot = Collection->GetPipeline()->GetSpecification()->Slots.Find(InSection.SlotName);
	if (!PipelineSlot)
	{
		return false;
	}

	const TArray<TSoftClassPtr<UObject>> SupportedTypes = PipelineSlot->SupportedPrincipalAssetTypes;
	TArray<TSubclassOf<UObject>> SupportedClasses;
	Algo::TransformIf(SupportedTypes, SupportedClasses,
		[](const TSoftClassPtr<UObject>& SoftClassPtr)
		{
			return SoftClassPtr.IsValid();
		},
		[](const TSoftClassPtr<UObject>& SoftClassPtr)
		{
			return SoftClassPtr.Get();
		});

	return Algo::FindByPredicate(MetaHumanEditorSettings->WardrobePaths,
		[InSection, SupportedClasses](const FMetaHumanCharacterAssetsSection& Section)
		{
			return
				Section.ContentDirectoryToMonitor.Path == InSection.ContentDirectoryToMonitor.Path &&
				Section.ClassesToFilter == SupportedClasses &&
				Section.SlotName == InSection.SlotName;
		}) != nullptr;
}

void SMetaHumanCharacterEditorWardrobeToolView::OnHandleWardrobeVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	UMetaHumanCharacterEditorWardrobeTool* WardrobeTool = Cast<UMetaHumanCharacterEditorWardrobeTool>(Tool);
	UMetaHumanCharacter* Character = WardrobeTool ? UE::ToolTarget::GetTargetMetaHumanCharacter(WardrobeTool->GetTarget()) : nullptr;
	if (!Character || !Item.IsValid())
	{
		return;
	}

	UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(Item->AssetData.GetAsset());
	if (!WardrobeItem)
	{
		return;
	}

	FMetaHumanCharacterWardrobeIndividualAssets& IndividualAssets = Character->WardrobeIndividualAssets.FindOrAdd(Item->SlotName);
	if (!IndividualAssets.Items.Contains(WardrobeItem))
	{
		Character->Modify();
		IndividualAssets.Items.Add(TNotNull<UMetaHumanWardrobeItem*>(WardrobeItem));
	}
}

bool SMetaHumanCharacterEditorWardrobeToolView::ApplyWearRequest(const TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>>& Items, EWearRequest WearRequest)
{
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());

	if (!WardrobeToolProperties)
	{
		return false;
	}

	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;

	if (!Collection)
	{
		return false;
	}

	const UMetaHumanCharacterPipelineSpecification* Specification = WardrobeToolProperties->Collection->GetPipeline()->GetSpecification();

	if (!Specification)
	{
		return false;
	}

	UMetaHumanCharacter* Character = WardrobeToolProperties->Character;
	
	// Items to process sorted by slot
	TMap<FName, TArray<TSharedRef<FMetaHumanCharacterAssetViewItem>>> ItemsPerSlot;

	bool bHasIncompatibleItems = false;

	for (const TSharedPtr<FMetaHumanCharacterAssetViewItem>& Item : Items)
	{
		const UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(Item->AssetData.GetAsset());

		// Check if we can use this slot
		if (!WardrobeItem
			|| !Collection->GetEditorPipeline()->IsWardrobeItemCompatibleWithSlot(Item->SlotName, WardrobeItem))
		{
			continue;
		}		

		UObject* WardrobeItemAsset = WardrobeItem->PrincipalAsset.LoadSynchronous();

		auto CreateBaseErrorMessageLog = [WardrobeItem, WardrobeItemAsset, Character]
		{
			const FText CantApplyText = LOCTEXT("CantApplyWardrobeItem", "Can't apply wardrobe item ");

			return FMessageLog(UE::MetaHuman::MessageLogName)
				.Error()
				->AddToken(FUObjectToken::Create(Character))
				->AddText(CantApplyText)
				->AddToken(FUObjectToken::Create(WardrobeItem))
				->AddToken(FUObjectToken::Create(WardrobeItemAsset));
		};

		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(WardrobeItemAsset))
		{
			USkeleton* Skeleton = SkeletalMesh->GetSkeleton();

			if (Skeleton == nullptr)
			{
				CreateBaseErrorMessageLog()
					->AddText(LOCTEXT("InvalidSkeleton", " has invalid skeleton"));

				bHasIncompatibleItems = true;

				continue;
			}

			const bool bIsBodyCompatible = UMetaHumanAssetManager::IsMetaHumanBodyCompatibleSkeleton(Skeleton);

			if (!bIsBodyCompatible)
			{
				CreateBaseErrorMessageLog()
					->AddText(LOCTEXT("IncompatibleSkeleton1", "The skeleton"))
					->AddToken(FUObjectToken::Create(Skeleton))
					->AddText(LOCTEXT("IncompatibleSkelton2", "is not compatible with the MetaHuman Body Skeleton"));

				bHasIncompatibleItems = true;

				continue;
			}
		}
		else if (UGroomBindingAsset* GroomBinding = Cast<UGroomBindingAsset>(WardrobeItemAsset))
		{
			UGroomAsset* GroomAsset = GroomBinding->GetGroom();

			if (GroomAsset == nullptr)
			{
				CreateBaseErrorMessageLog()
					->AddText(LOCTEXT("InvalidGroomAsset", "has invalid groom asset"));

				bHasIncompatibleItems = true;

				continue;
			}

			USkeletalMesh* SourceGroomMesh = GroomBinding->GetSourceSkeletalMesh();
			USkeletalMesh* TargetGroomMesh = GroomBinding->GetTargetSkeletalMesh();

			if (SourceGroomMesh == nullptr)
			{
				CreateBaseErrorMessageLog()
					->AddText(LOCTEXT("InvalidSourceMesh", "has invalid source mesh"));

				bHasIncompatibleItems = true;

				continue;
			}

			if (TargetGroomMesh == nullptr)
			{
				CreateBaseErrorMessageLog()
					->AddText(LOCTEXT("InvalidTargetMesh", "has invalid target mesh"));

				bHasIncompatibleItems = true;

				continue;
			}

			const FBoxSphereBounds& SourceMeshBounds = SourceGroomMesh->GetBounds();
			const FBoxSphereBounds GroomBounds{ GroomAsset->GetHairDescriptionGroups().Bounds };

			if (!SourceMeshBounds.SphereRadius || !FBoxSphereBounds::BoxesIntersect(SourceMeshBounds, GroomBounds))
			{
				CreateBaseErrorMessageLog()
					->AddToken(FUObjectToken::Create(GroomAsset))
					->AddText(LOCTEXT("DoesntOverlap1", "does not overlap spatially with "))
					->AddToken(FUObjectToken::Create(SourceGroomMesh))
					->AddText(LOCTEXT("DoestOverlap2", "so it cannot be used as a source mesh"));

				bHasIncompatibleItems = true;

				continue;
			}

			const FTriangleArray& SourceTriangles = SourceGroomMesh->GetMeshDescription(0)->Triangles();
			if (SourceTriangles.Num() != 64094)
			{
				CreateBaseErrorMessageLog()
					->AddToken(FUObjectToken::Create(SourceGroomMesh))
					->AddText(LOCTEXT("InvalidTopology", "Does not match the standard MetaHuman Topology"));

				bHasIncompatibleItems = true;

				continue;
			}
		}

		ItemsPerSlot.FindOrAdd(Item->SlotName).Add(Item.ToSharedRef());
	}

	if (bHasIncompatibleItems)
	{
		FMessageLog(UE::MetaHuman::MessageLogName).Open();
	}

	if (ItemsPerSlot.IsEmpty())
	{
		return false;
	}

	auto TryPrepareItem = [this](const TSharedRef<FMetaHumanCharacterAssetViewItem>& ItemToPrepare)
	{
		if (ItemToPrepare->PaletteItemKey.IsNull())
		{
			// This item isn't in the palette yet, so add it now
			PrepareAsset(ItemToPrepare);

			if (ItemToPrepare->PaletteItemKey.IsNull())
			{
				UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to prepare item {WardrobeItem}",
					GetFullNameSafe(ItemToPrepare->AssetData.GetAsset()));
			}
		}
	};

	TNotNull<UMetaHumanCharacterInstance*> Instance = Collection->GetMutableDefaultInstance();
	bool bCollectionUpdated = false;

#define RECORD_WARDROBE_ITEM_WORN_EVENT()\
	UE::MetaHuman::Analytics::RecordWardrobeItemWornEvent(SlotName, Item->AssetData.GetAsset()->GetFName())

	for (const TPair<FName, TArray<TSharedRef<FMetaHumanCharacterAssetViewItem>>>& Kvp : ItemsPerSlot)
	{
		const FName SlotName = Kvp.Key;
		
		if (const FMetaHumanCharacterPipelineSlot* SlotSpec = Specification->Slots.Find(SlotName))
		{
			if (SlotSpec->bAllowsMultipleSelection)
			{
				for (const TSharedRef<FMetaHumanCharacterAssetViewItem>& Item : Kvp.Value)
				{
					switch (WearRequest)
					{
						case EWearRequest::Wear:
						{
							TryPrepareItem(Item);
							const FMetaHumanPipelineSlotSelection SlotSelectionItem(SlotName, Item->PaletteItemKey);

							if (Instance->TryAddSlotSelection(SlotSelectionItem))
							{
								bCollectionUpdated = true;
								RECORD_WARDROBE_ITEM_WORN_EVENT();
							}
							break;
						}
						case EWearRequest::Unwear:
						{
							const FMetaHumanPipelineSlotSelection SlotSelectionItem(SlotName, Item->PaletteItemKey);
							if (Instance->TryRemoveSlotSelection(SlotSelectionItem))
							{
								bCollectionUpdated = true;
							}
							break;
						}
						case EWearRequest::Toggle:
						{
							TryPrepareItem(Item);
							const FMetaHumanPipelineSlotSelection SlotSelectionItem(SlotName, Item->PaletteItemKey);

							if (!Instance->TryRemoveSlotSelection(SlotSelectionItem))
							{
								// If the item couldn't be removed, adding it should succeed
								if (!Instance->TryAddSlotSelection(SlotSelectionItem))
								{
									UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to add item {WardrobeItem} to slot {SlotName}",
										GetFullNameSafe(Item->AssetData.GetAsset()), SlotName.ToString());
								}
								else
								{
									bCollectionUpdated = true;
									RECORD_WARDROBE_ITEM_WORN_EVENT();
								}
							}
							else
							{
								bCollectionUpdated = true;
							}
							break;
						}
					}
				}
			}
			else
			{
				switch (WearRequest)
				{
					case EWearRequest::Wear:
					{
						// Single slot selection, so just pick the last item and skip the rest
						TSharedRef<FMetaHumanCharacterAssetViewItem> Item = Kvp.Value.Last();
						TryPrepareItem(Item);
						Instance->SetSingleSlotSelection(SlotName, Item->PaletteItemKey);
						bCollectionUpdated = true;
						RECORD_WARDROBE_ITEM_WORN_EVENT();
						break;
					}
					case EWearRequest::Unwear:
					{
						FMetaHumanPaletteItemKey CurrentSelectionItemKey;

						if (Instance->TryGetAnySlotSelection(SlotName, CurrentSelectionItemKey))
						{
							const bool bHasItem = Kvp.Value.ContainsByPredicate(
								[CurrentSelectionItemKey](const TSharedRef<FMetaHumanCharacterAssetViewItem>& Item)
								{
									return CurrentSelectionItemKey == Item->PaletteItemKey;
								});

							if (bHasItem)
							{
								Instance->SetSingleSlotSelection(SlotName, FMetaHumanPaletteItemKey());
								bCollectionUpdated = true;
								break;
							}
						}
						break;
					}
					case EWearRequest::Toggle:
					{
						// Single slot selection, so just pick the last item and skip the rest
						TSharedRef<FMetaHumanCharacterAssetViewItem> Item = Kvp.Value.Last();
						const FMetaHumanPipelineSlotSelection SlotSelectionItem(SlotName, Item->PaletteItemKey);

						if (!Instance->TryRemoveSlotSelection(SlotSelectionItem))
						{
							TryPrepareItem(Item);
							Instance->SetSingleSlotSelection(SlotName, Item->PaletteItemKey);
							RECORD_WARDROBE_ITEM_WORN_EVENT();
						}

						bCollectionUpdated = true;

						break;
					}
				}
			}
		}
	}

	if (!bCollectionUpdated)
	{
		return false;
	}

	BuildCollection();

	return true;
}

void SMetaHumanCharacterEditorWardrobeToolView::BuildCollection()
{
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;

	if (!Collection)
	{
		return;
	}

	FInstancedStruct BuildInput;
	if (UMetaHumanCharacter* MetaHumanCharacter = Collection->GetTypedOuter<UMetaHumanCharacter>())
	{
		const TObjectPtr<UScriptStruct> BuildInputStruct = MetaHumanCharacter->GetInternalCollection()->GetEditorPipeline()->GetSpecification()->BuildInputStruct;
		if (BuildInputStruct
			&& BuildInputStruct->IsChildOf(FMetaHumanBuildInputBase::StaticStruct()))
		{
			// Initialize to the struct that the pipeline is expecting.
			//
			// Any properties defined in sub-structs of FMetaHumanBuildInputBase will be left as
			// their default values.
			BuildInput.InitializeAs(BuildInputStruct);

			FMetaHumanBuildInputBase& TypedBuildInput = BuildInput.GetMutable<FMetaHumanBuildInputBase>();
			TypedBuildInput.EditorPreviewCharacter = MetaHumanCharacter->GetInternalCollectionKey();


			Collection->Build(
				BuildInput,
				EMetaHumanCharacterPaletteBuildQuality::Preview,
				GetTargetPlatformManagerRef().GetRunningTargetPlatform(),
				UMetaHumanCollection::FOnBuildComplete(),
				Collection->GetDefaultInstance()->ToPinnedSlotSelections(EMetaHumanUnusedSlotBehavior::PinnedToEmpty));

			MetaHumanCharacter->MarkPackageDirty();
		}
	}
}

void SMetaHumanCharacterEditorWardrobeToolView::PrepareAsset(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;
	if (!Collection || !Item.IsValid())
	{
		return;
	}

	const FName SlotName = Item->SlotName;

	// First check if the asset is already prepared
	const FMetaHumanCharacterPaletteItem* FoundItem = Collection->GetItems().FindByPredicate(
		[&WardrobeItemAssetData = Item->AssetData, SlotName](const FMetaHumanCharacterPaletteItem& Item)
		{
			return Item.SlotName == SlotName
				&& Item.WardrobeItem
				&& Item.WardrobeItem->IsExternal()
				&& FSoftObjectPath(Item.WardrobeItem) == WardrobeItemAssetData.ToSoftObjectPath();
		});

	if (FoundItem)
	{
		// Found an existing prepared asset
		Item->PaletteItemKey = FoundItem->GetItemKey();
		return;
	}

	UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(Item->AssetData.GetAsset());
	if (!WardrobeItem)
	{
		return;
	}

	if (!Collection->TryAddItemFromWardrobeItem(SlotName, WardrobeItem, Item->PaletteItemKey))
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to prepare asset {WardrobeItem}", GetFullNameSafe(WardrobeItem));
	}

	UE::MetaHuman::Analytics::RecordWardrobeItemPreparedEvent(Item->SlotName, Item->AssetData.GetAsset()->GetFName());
}

void SMetaHumanCharacterEditorWardrobeToolView::UnprepareAsset(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	UMetaHumanCharacterEditorWardrobeToolProperties* WardrobeToolProperties = Cast<UMetaHumanCharacterEditorWardrobeToolProperties>(GetToolProperties());
	UMetaHumanCollection* Collection = WardrobeToolProperties ? WardrobeToolProperties->Collection : nullptr;
	if (!Collection || !Item.IsValid() || Item->PaletteItemKey.IsNull() )
	{
		return;
	}
	
	TNotNull<UMetaHumanCharacterInstance*> Instance = Collection->GetMutableDefaultInstance();

	FMetaHumanPaletteItemKey CurrentSelectionItemKey;

	// Don't allow asset unprepare if character is wearing that item
	if (Instance->TryGetAnySlotSelection(Item->SlotName, CurrentSelectionItemKey))
	{
		if (CurrentSelectionItemKey == Item->PaletteItemKey)
		{
			return;
		}
	}

	ensure(Collection->TryRemoveItem(Item->PaletteItemKey));

	Instance->TryRemoveSlotSelection(
		FMetaHumanPipelineSlotSelection(
			Item->SlotName,
			Item->PaletteItemKey));

	Item->PaletteItemKey.Reset();
}

void SMetaHumanCharacterEditorWardrobeToolView::OnWardrobePathsChanged()
{
	if (AssetViewsPanel.IsValid())
	{
		AssetViewsPanel->RequestRefresh();
	}
}

void SMetaHumanCharacterEditorWardrobeToolView::OnPrepareAccessory()
{
	if (!AssetViewsPanel.IsValid())
	{
		return;
	}

	bool bShouldUpdate = false;
	const TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> SelectedItems = AssetViewsPanel->GetSelectedItems();
	for (const TSharedPtr<FMetaHumanCharacterAssetViewItem>& SelectedItem : SelectedItems)
	{
		if (SelectedItem.IsValid() && SelectedItem->PaletteItemKey.IsNull())
		{
			PrepareAsset(SelectedItem);
			bShouldUpdate |= !SelectedItem->PaletteItemKey.IsNull();
		}
	}

	if (bShouldUpdate)
	{
		BuildCollection();

		AssetViewsPanel->RequestRefresh();
	}
}

void SMetaHumanCharacterEditorWardrobeToolView::OnUnprepareAccessory()
{
	if (!AssetViewsPanel.IsValid())
	{
		return;
	}

	bool bShouldUpdate = false;
	const TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> SelectedItems = AssetViewsPanel->GetSelectedItems();
	for (const TSharedPtr<FMetaHumanCharacterAssetViewItem>& SelectedItem : SelectedItems)
	{
		if (SelectedItem.IsValid() && !SelectedItem->PaletteItemKey.IsNull())
		{
			UnprepareAsset(SelectedItem);
			bShouldUpdate |= SelectedItem->PaletteItemKey.IsNull();
		}
	}

	if (bShouldUpdate)
	{
		BuildCollection();
		
		AssetViewsPanel->RequestRefresh();
	}
}

void SMetaHumanCharacterEditorWardrobeToolView::OnWearAccessory()
{
	if (AssetViewsPanel.IsValid())
	{
		if (ApplyWearRequest(AssetViewsPanel->GetSelectedItems(), EWearRequest::Wear))
		{
			AssetViewsPanel->RequestRefresh();
		}
	}
}

void SMetaHumanCharacterEditorWardrobeToolView::OnRemoveAccessory()
{
	if (AssetViewsPanel.IsValid())
	{
		if (ApplyWearRequest(AssetViewsPanel->GetSelectedItems(), EWearRequest::Unwear))
		{
			AssetViewsPanel->RequestRefresh();
		}
	}
}

void SMetaHumanCharacterEditorWardrobeToolView::OnOpenAccessoryProperties()
{
	if (!AssetViewsPanel.IsValid())
	{
		return;
	}

	TArray<UObject*> Assets;

	Algo::Transform(
		AssetViewsPanel->GetSelectedItems(),
		Assets,
		[](const TSharedPtr<FMetaHumanCharacterAssetViewItem>& Item)
		{
			return Item->AssetData.GetAsset();
		});

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(Assets);
}

#undef LOCTEXT_NAMESPACE
