// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorPresetsToolView.h"

#include "Algo/RemoveIf.h"
#include "AssetThumbnail.h"
#include "Brushes/SlateImageBrush.h" 
#include "ContentBrowserItem.h"
#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/SToolBarButtonBlock.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "MetaHumanCharacterEditorCommands.h"
#include "MetaHumanCharacterAssetObserver.h"
#include "MetaHumanCharacterEditorSettings.h"
#include "MetaHumanCharacterEditorWardrobeSettings.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "MetaHumanCharacterEditorModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SPrimaryButton.h"
#include "Tools/MetaHumanCharacterEditorPresetsTool.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "UI/Widgets/SMetaHumanCharacterEditorAssetViewsPanel.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorPresetsToolView"

FName SMetaHumanCharacterEditorPresetsToolView::PresetsLibraryAssetsSlotName = FName(TEXT("Presets Library"));

void SMetaHumanCharacterEditorPresetsToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorPresetsTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorPresetsToolView::GetToolProperties() const
{
	const UMetaHumanCharacterEditorPresetsTool* PresetsTool = Cast<UMetaHumanCharacterEditorPresetsTool>(Tool);
	return IsValid(PresetsTool) ? PresetsTool->GetPresetsToolProperties() : nullptr;
}

void SMetaHumanCharacterEditorPresetsToolView::MakeToolView()
{
	if (ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreatePresetsToolViewPresetsViewSection()
				]
			];
	}

	if (ToolViewMainBox.IsValid())
	{
		ToolViewMainBox->AddSlot()
			.Padding(0.f, 4.f, 0.f, 0.f)
			.AutoHeight()
			[
				CreatePresetsToolViewManagementSection()
			];

		ToolViewMainBox->AddSlot()
			.Padding(0.f, 4.f, 0.f, 0.f)
			.AutoHeight()
			[
				CreatePresetsToolViewLibrarySection()
			];
	}
}

void SMetaHumanCharacterEditorPresetsToolView::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	OnPreEditChangeProperty(PropertyAboutToChange, PropertyAboutToChange->GetName());
}

void SMetaHumanCharacterEditorPresetsToolView::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	const bool bIsInteractive = PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive;
	OnPostEditChangeProperty(PropertyThatChanged, bIsInteractive);
}

TSharedRef<SWidget> SMetaHumanCharacterEditorPresetsToolView::CreatePresetsToolViewPresetsViewSection()
{
	UMetaHumanCharacterEditorPresetsTool* PresetsTool = Cast<UMetaHumanCharacterEditorPresetsTool>(Tool);	
	TWeakObjectPtr<UMetaHumanCharacter> Character = UE::ToolTarget::GetTargetMetaHumanCharacter(PresetsTool->GetTarget());
	if (!Character.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	UMetaHumanCharacterEditorSettings* MetaHumanEditorSettings = GetMutableDefault<UMetaHumanCharacterEditorSettings>();
	if (!MetaHumanEditorSettings->GetOnPresetsDirectoriesChanged().IsBoundToObject(this))
	{
		MetaHumanEditorSettings->GetOnPresetsDirectoriesChanged().BindSP(this, &SMetaHumanCharacterEditorPresetsToolView::OnPresetsDirectoriesChanged);
	}

	const TSharedRef<SWidget> PresetsViewSectionWidget =
		SNew(SVerticalBox)

		// Presets View section
		+ SVerticalBox::Slot()
		.Padding(4.f, 10.f)
		.AutoHeight()
		[
			SNew(SOverlay)

			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				
				+ SVerticalBox::Slot()
				.Padding(-2.f)
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(30.f)
					[
						SNew(SImage)
						.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.Rounded.DefaultBrush")))
					]
				]

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				[
					SNew(SBox)
				]
			]

			+ SOverlay::Slot()
			.Padding(0.f, 2.f)
			[
				SAssignNew(AssetViewsPanel, SMetaHumanCharacterEditorAssetViewsPanel)
				.AutoHeight(true)
				.AllowDragging(false)
				.AllowSlots(false)
				.AllowMultiSelection(false)
				.AllowSlotMultiSelection(false)
				.AssetViewSections(this, &SMetaHumanCharacterEditorPresetsToolView::GetAssetViewsSections)
				.ExcludedObjects({ Character.Get() })
				.VirtualFolderClassesToFilter({ UMetaHumanCharacter::StaticClass() })
				.OnPopulateAssetViewsItems(this, &SMetaHumanCharacterEditorPresetsToolView::OnPopulateAssetViewsItems)
				.OnProcessDroppedFolders(this, &SMetaHumanCharacterEditorPresetsToolView::OnProcessDroppedFolders)
				.OnItemActivated(this, &SMetaHumanCharacterEditorPresetsToolView::OnPresetsToolItemActivated)
				.OnItemDeleted(this, &SMetaHumanCharacterEditorPresetsToolView::OnPresetsToolVirtualItemDeleted)
				.CanDeleteItem(this, &SMetaHumanCharacterEditorPresetsToolView::CanDeletePresetsToolVirtualItem)
				.OnFolderDeleted(this, &SMetaHumanCharacterEditorPresetsToolView::OnPresetsPathsFolderDeleted)
				.CanDeleteFolder(this, &SMetaHumanCharacterEditorPresetsToolView::CanDeletePresetsPathsFolder)
				.OnHadleVirtualItem(this, &SMetaHumanCharacterEditorPresetsToolView::OnHandlePresetsVirtualItem)
			]
		];

	return PresetsViewSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorPresetsToolView::CreatePresetsToolViewManagementSection()
{
	const FMetaHumanCharacterEditorToolCommands& Commands = FMetaHumanCharacterEditorToolCommands::Get();
	CommandList = MakeShareable(new FUICommandList);

	CommandList->MapAction
	(
		Commands.PresetProperties,
		FExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorPresetsToolView::OpenPresetPropertiesWindow),
		FCanExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorPresetsToolView::IsPropertiesEditingEnabled),
		FIsActionChecked::CreateLambda([this]()
			{
				return PresetsPropertiesWindow.IsValid();
			})
	);

	CommandList->MapAction
	(
		Commands.ApplyPreset,
		FExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorPresetsToolView::ApplyPreset),
		FCanExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorPresetsToolView::IsPropertiesEditingEnabled)
	);

	const TSharedRef<FSlimHorizontalUniformToolBarBuilder> ToolbarBuilder = MakeShareable(new FSlimHorizontalUniformToolBarBuilder(CommandList, FMultiBoxCustomization(TEXT("SlimHorizontal"))));
	ToolbarBuilder->SetStyle(&FAppStyle::Get(), "SlimPaletteToolBar");

	FButtonArgs PresetPropertiesArgs;
	PresetPropertiesArgs.CommandList = CommandList;
	PresetPropertiesArgs.Command = Commands.PresetProperties;
	// TODO: Enable the button when preset properties are implemented
	// ToolbarBuilder->AddToolBarButton(PresetPropertiesArgs);
		
	FButtonArgs ApplyPresetArgs;
	ApplyPresetArgs.CommandList = CommandList;
	ApplyPresetArgs.Command = Commands.ApplyPreset;
	ToolbarBuilder->AddToolBarButton(ApplyPresetArgs);

	const TSharedRef<SWidget> ManagementSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("ManagementSectionLabel", "Presets Management"))
		.Padding(-4.f)
		.RoundedBorders(false)
		.Content()
		[
			SNew(SVerticalBox)

			// Presets Management buttons section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(4.f, 4.f, 4.f, 8.f)
			.AutoHeight()
			[
				ToolbarBuilder->MakeWidget()
			]
		];

	return ManagementSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorPresetsToolView::CreatePresetsToolViewLibrarySection()
{
	UMetaHumanCharacterEditorPresetsToolProperties* PrestsToolProperties = Cast<UMetaHumanCharacterEditorPresetsToolProperties>(GetToolProperties());
	if (!PrestsToolProperties)
	{
		return SNullWidget::NullWidget;
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = true;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = true;
	StructureViewArgs.bShowInterfaces = true;

	TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FMetaHumanCharacterPresetsLibraryProperties::StaticStruct(), (uint8*)&PrestsToolProperties->LibraryManagement);
	TSharedRef<IStructureDetailsView> StructDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, StructOnScope);

	const TSharedRef<SWidget> LibrarySectionWidget =
		SNew(SBorder)
		.Padding(-4.f)
		.BorderImage(FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.ActiveToolLabel"))
		.Visibility(EVisibility::Collapsed)
		[
			SNew(SVerticalBox)

			// Library Management details view section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				StructDetailsView->GetWidget().ToSharedRef()
			]

			// Inspect button section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(50.f)
				.HAlign(HAlign_Fill)
				.Padding(10.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
					.ForegroundColor(FLinearColor::White)
					.IsEnabled(this, &SMetaHumanCharacterEditorPresetsToolView::IsInspectButtonEnabled)
					.OnClicked(this, &SMetaHumanCharacterEditorPresetsToolView::OnInspectButtonClicked)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("InspectPathPresetsToolButton", "Inspect Path"))
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]
			]
		];

	return LibrarySectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorPresetsToolView::MakePresetPropertiesWindow()
{
	UMetaHumanCharacterEditorPresetsToolProperties* PrestsToolProperties = Cast<UMetaHumanCharacterEditorPresetsToolProperties>(GetToolProperties());
	if (!PrestsToolProperties)
	{
		return SNullWidget::NullWidget;
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = true;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = true;
	StructureViewArgs.bShowInterfaces = true;

	TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FMetaHumanCharacterPresetsManagementProperties::StaticStruct(), (uint8*)&PrestsToolProperties->PresetsManagement);
	TSharedRef<IStructureDetailsView> StructDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, StructOnScope);

	const TSharedRef<SWidget> PresetPropertiesContent =
		SNew(SVerticalBox)
		
		// Details View section
		+ SVerticalBox::Slot()
		.FillHeight(.8f)
		[
			StructDetailsView->GetWidget().ToSharedRef()
		]

		// Dialog section
		+ SVerticalBox::Slot()
		.FillHeight(.2f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(10.f, 0.f)
			[
				SNew(SHorizontalBox)
					
				// Accept button section
				+SHorizontalBox::Slot()
				[
					SNew(SPrimaryButton)
					.OnClicked(this, &SMetaHumanCharacterEditorPresetsToolView::OnAcceptPresetPropertiesClicked)
					.Text(LOCTEXT("PresetsPropertiesWindowAcceptButton", "Accept"))
				]

				// Cancel button section
				+ SHorizontalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &SMetaHumanCharacterEditorPresetsToolView::OnCancelPresetPropertiesClicked)
					.Text(LOCTEXT("PresetsPropertiesWindowCancelButton", "Cancel"))
				]
			]
		];

	return PresetPropertiesContent;
}

FReply SMetaHumanCharacterEditorPresetsToolView::OnAcceptPresetPropertiesClicked()
{
	if (PresetsPropertiesWindow.IsValid())
	{
		FSlateApplication::Get().RequestDestroyWindow(PresetsPropertiesWindow.ToSharedRef());
	}

	//TODO register presets properties?

	return FReply::Handled();
}

FReply SMetaHumanCharacterEditorPresetsToolView::OnCancelPresetPropertiesClicked()
{
	if (PresetsPropertiesWindow.IsValid())
	{
		FSlateApplication::Get().RequestDestroyWindow(PresetsPropertiesWindow.ToSharedRef());
	}

	//TODO reset presets properties?

	return FReply::Handled();
}

bool SMetaHumanCharacterEditorPresetsToolView::IsPropertiesEditingEnabled() const
{
	return AssetViewsPanel.IsValid() && !AssetViewsPanel->GetSelectedItems().IsEmpty();
}

bool SMetaHumanCharacterEditorPresetsToolView::IsInspectButtonEnabled() const
{
	// TODO maybe add valid path conditions?

	const UMetaHumanCharacterEditorPresetsToolProperties* PrestsToolProperties = Cast<UMetaHumanCharacterEditorPresetsToolProperties>(GetToolProperties());
	return PrestsToolProperties && PrestsToolProperties->LibraryManagement.ProjectPath.Path.IsEmpty();
}

FReply SMetaHumanCharacterEditorPresetsToolView::OnInspectButtonClicked()
{
	const UMetaHumanCharacterEditorPresetsToolProperties* PrestsToolProperties = Cast<UMetaHumanCharacterEditorPresetsToolProperties>(GetToolProperties());
	UMetaHumanCharacterEditorSettings* Settings = GetMutableDefault<UMetaHumanCharacterEditorSettings>();
	if (Settings && PrestsToolProperties)
	{
		FProperty* PresetsDirectoriesProperty = UMetaHumanCharacterEditorSettings::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, PresetsDirectories));
		Settings->PreEditChange(PresetsDirectoriesProperty);

		const FDirectoryPath& NewPath = PrestsToolProperties->LibraryManagement.ProjectPath;
		Settings->PresetsDirectories.Add(NewPath);

		FPropertyChangedEvent PropertyChangedEvent(PresetsDirectoriesProperty, EPropertyChangeType::ValueSet);
		Settings->PostEditChangeProperty(PropertyChangedEvent);
	}

	return FReply::Handled();
}

void SMetaHumanCharacterEditorPresetsToolView::OpenPresetPropertiesWindow()
{
	if (PresetsPropertiesWindow.IsValid())
	{
		PresetsPropertiesWindow->BringToFront();
		return;
	}

	const FText TitleText = LOCTEXT("PresetsPropertiesWindow", "Presets Properties");

	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(TitleText)
		.HasCloseButton(true)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(400.0f, 200.0f))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(true)
		.SupportsMaximize(true)
		[
			MakePresetPropertiesWindow()
		];

	Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda(
		[this](const TSharedRef<SWindow>& InWindow)
		{
			PresetsPropertiesWindow = nullptr;
		}));

	PresetsPropertiesWindow = Window;
	FSlateApplication::Get().AddWindow(Window);
}

void SMetaHumanCharacterEditorPresetsToolView::ApplyPreset()
{
	TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> Presets = AssetViewsPanel->GetSelectedItems();
	if (Presets.Num() > 0)
	{
		const TSharedPtr<FMetaHumanCharacterAssetViewItem> FirstPreset = Presets[0];
		if (FirstPreset.IsValid())
		{
			UMetaHumanCharacter* PresetCharacter = Cast<UMetaHumanCharacter>(FirstPreset->AssetData.GetAsset());
			UMetaHumanCharacterEditorPresetsTool* PresetsTool = Cast<UMetaHumanCharacterEditorPresetsTool>(Tool);
			PresetsTool->ApplyPresetCharacter(PresetCharacter);		
		}
	}
}

TArray<FMetaHumanCharacterAssetViewItem> SMetaHumanCharacterEditorPresetsToolView::GetCharacterIndividualAssets() const
{
	TArray<FMetaHumanCharacterAssetViewItem> Items;

	UMetaHumanCharacterEditorPresetsTool* PresetsTool = Cast<UMetaHumanCharacterEditorPresetsTool>(Tool);
	const UMetaHumanCharacter* Character = PresetsTool ? UE::ToolTarget::GetTargetMetaHumanCharacter(PresetsTool->GetTarget()) : nullptr;
	if (!Character)
	{
		return Items;
	}

	const FMetaHumanCharacterIndividualAssets* IndividualAssets = Character->CharacterIndividualAssets.Find(PresetsLibraryAssetsSlotName);
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

TArray<FMetaHumanCharacterAssetsSection> SMetaHumanCharacterEditorPresetsToolView::GetAssetViewsSections() const
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

TArray<FMetaHumanCharacterAssetViewItem> SMetaHumanCharacterEditorPresetsToolView::OnPopulateAssetViewsItems(const FMetaHumanCharacterAssetsSection& InSection, const FMetaHumanObserverChanges& InChanges)
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
		Items.Add(FMetaHumanCharacterAssetViewItem(Asset, InSection.SlotName, FMetaHumanPaletteItemKey(), nullptr));
	}

	return Items;
}

void SMetaHumanCharacterEditorPresetsToolView::OnProcessDroppedFolders(const TArray<FContentBrowserItem> Items, const FMetaHumanCharacterAssetsSection& InSection) const
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

void SMetaHumanCharacterEditorPresetsToolView::OnPresetsToolItemActivated(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	if (Item.IsValid())
	{
		ApplyPreset();
	}
}

void SMetaHumanCharacterEditorPresetsToolView::OnPresetsToolVirtualItemDeleted(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	UMetaHumanCharacterEditorPresetsTool* PresetsTool = Cast<UMetaHumanCharacterEditorPresetsTool>(Tool);
	UMetaHumanCharacter* Character = PresetsTool ? UE::ToolTarget::GetTargetMetaHumanCharacter(PresetsTool->GetTarget()) : nullptr;
	if (!Character || !Item.IsValid())
	{
		return;
	}

	UMetaHumanCharacter* CharacterItem = Cast<UMetaHumanCharacter>(Item->AssetData.GetAsset());
	FMetaHumanCharacterIndividualAssets* IndividualAssets = Character->CharacterIndividualAssets.Find(PresetsLibraryAssetsSlotName);
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

bool SMetaHumanCharacterEditorPresetsToolView::CanDeletePresetsToolVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const
{
	UMetaHumanCharacterEditorPresetsTool* PresetsTool = Cast<UMetaHumanCharacterEditorPresetsTool>(Tool);
	UMetaHumanCharacter* Character = PresetsTool ? UE::ToolTarget::GetTargetMetaHumanCharacter(PresetsTool->GetTarget()) : nullptr;
	if (!Character || !Item.IsValid() || !Item->AssetData.IsAssetLoaded())
	{
		return false;
	}

	UMetaHumanCharacter* CharacterItem = Cast<UMetaHumanCharacter>(Item->AssetData.GetAsset());
	FMetaHumanCharacterIndividualAssets* IndividualAssets = Character->CharacterIndividualAssets.Find(PresetsLibraryAssetsSlotName);
	if (!CharacterItem || !IndividualAssets)
	{
		return false;
	}

	return IndividualAssets->Characters.Contains(CharacterItem);
}

void SMetaHumanCharacterEditorPresetsToolView::OnPresetsPathsFolderDeleted(const FMetaHumanCharacterAssetsSection& InSection)
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

bool SMetaHumanCharacterEditorPresetsToolView::CanDeletePresetsPathsFolder(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item, const FMetaHumanCharacterAssetsSection& InSection) const
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

void SMetaHumanCharacterEditorPresetsToolView::OnHandlePresetsVirtualItem(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	UMetaHumanCharacterEditorPresetsTool* PresetsTool = Cast<UMetaHumanCharacterEditorPresetsTool>(Tool);
	UMetaHumanCharacter* Character = PresetsTool ? UE::ToolTarget::GetTargetMetaHumanCharacter(PresetsTool->GetTarget()) : nullptr;
	if (!Character || !Item.IsValid() || Item->AssetData.AssetClassPath.ToString() != UMetaHumanCharacter::StaticClass()->GetPathName())
	{
		return;
	}

	UMetaHumanCharacter* CharacterItem = Cast<UMetaHumanCharacter>(Item->AssetData.GetAsset());
	if (!CharacterItem)
	{
		return;
	}

	FMetaHumanCharacterIndividualAssets& IndividualAssets = Character->CharacterIndividualAssets.FindOrAdd(PresetsLibraryAssetsSlotName);
	if (!IndividualAssets.Characters.Contains(CharacterItem))
	{
		Character->Modify();
		IndividualAssets.Characters.Add(TNotNull<UMetaHumanCharacter*>(CharacterItem));
	}
}

void SMetaHumanCharacterEditorPresetsToolView::OnPresetsDirectoriesChanged()
{
	if (AssetViewsPanel.IsValid())
	{
		AssetViewsPanel->RequestRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
