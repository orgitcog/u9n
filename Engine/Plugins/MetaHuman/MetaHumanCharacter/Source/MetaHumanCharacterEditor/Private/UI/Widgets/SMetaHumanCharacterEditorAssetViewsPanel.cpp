// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorAssetViewsPanel.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Transform.h"
#include "AssetThumbnail.h"
#include "AssetToolsModule.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserDataDragDropOp.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "Engine/TimerHandle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/SlateDelegates.h"
#include "HAL/FileManager.h"
#include "ISettingsModule.h"
#include "Layout/WidgetPath.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterAssetObserver.h"
#include "MetaHumanCharacterEditorSettings.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Slate/DeferredCleanupSlateBrush.h"
#include "Styling/StyleColors.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "FileHelpers.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorAssetViewsPanel"

namespace UE::MetaHuman::Private
{
	static const FText GenericSlotText = LOCTEXT("MetaHumanAssetViewsPanel_GenericSlot_Label", "Others");
	static const FText VirtualFolderText = LOCTEXT("MetaHumanAssetViewsPanel_VirtualFolder_Label", "Individual Assets");
	static const FText MultiFolderText = LOCTEXT("MetaHumanAssetViewsPanel_MultiFolder_Label", "All Assets");

	static constexpr float TileSize = 128.f;
	static constexpr float ThumbnailSize = 112.f;
	static constexpr float TileThumbnailPadding = 1.8f;
	static constexpr float TileTextSize = 34.f;
	static constexpr int32 ThumbnailPoolSize = 128;
}

FMetaHumanCharacterAssetViewItem::FMetaHumanCharacterAssetViewItem(const FAssetData& InAssetData, const FName& InSlotName, const FMetaHumanPaletteItemKey& InPaletteItemKey, const TSharedPtr<FAssetThumbnailPool>& InAssetThumbnailPool)
	: AssetData(InAssetData)
	, SlotName(InSlotName)
	, PaletteItemKey(InPaletteItemKey)
{
	if (InAssetThumbnailPool)
	{
		Thumbnail = MakeShared<FAssetThumbnail>(
			AssetData,
			UE::MetaHuman::Private::ThumbnailSize,
			UE::MetaHuman::Private::ThumbnailSize,
			InAssetThumbnailPool);
	}
}

FMetaHumanCharacterAssetViewItem::~FMetaHumanCharacterAssetViewItem()
{
	if (ThumbnailImageOverride.IsValid())
	{
		ThumbnailImageOverride.Reset();
		ThumbnailImageOverride = nullptr;
	}
}

TSharedRef<FMetaHumanCharacterAssetViewItemDragDropOp> FMetaHumanCharacterAssetViewItemDragDropOp::New(FAssetData AssetData, UActorFactory* ActorFactory, TSharedPtr<FMetaHumanCharacterAssetViewItem> AssetItem)
{
	// Create the drag-drop op containing the key
	TSharedRef<FMetaHumanCharacterAssetViewItemDragDropOp> Operation = MakeShareable(new FMetaHumanCharacterAssetViewItemDragDropOp);
	Operation->Init({ AssetData }, TArray<FString>(), ActorFactory);
	Operation->AssetItem = AssetItem;
	Operation->Construct();

	return Operation;
}

void SMetaHumanCharacterAssetViewItem::Construct(const FArguments& InArgs)
{
	AssetThumbnail = InArgs._AssetThumbnail;
	AssetItem = InArgs._AssetItem;

	IsSelected = InArgs._IsSelected;
	IsChecked = InArgs._IsChecked;
	IsAvailable = InArgs._IsAvailable;
	IsActive = InArgs._IsActive;

	OnOverrideThumbnailNameDelegate = InArgs._OnOverrideThumbnailName;
	OnOverrideThumbnailDelegate = InArgs._OnOverrideThumbnail;
	OnDeletedDelegate = InArgs._OnDeleted;
	CanDeleteDelegate = InArgs._CanDelete;
	OnDeletedFolderDelegate = InArgs._OnDeletedFolder;
	CanDeleteFolderDelegate = InArgs._CanDeleteFolder;

	Thumbnail = GenerateThumbnailWidget();

	using namespace UE::MetaHuman::Private;

	constexpr float AssetViewWidgetsBorderPadding = 4.f;
	constexpr float AssetViewWidgetsShadowPadding = 5.f;
	constexpr float AssetViewWidgetsNameWidgetPadding = 8.f;
	constexpr float MaxHeightNameArea = 128.f;
	constexpr FLinearColor StateOverlayBorderColor = FLinearColor(0.015f, 0.015f, 0.015f, .8f);

	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	const TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(AssetItem->AssetData.GetClass()).Pin();
	FLinearColor AssetColor = FLinearColor::White;
	if (AssetTypeActions.IsValid())
	{
		AssetColor = AssetTypeActions->GetTypeColor();
	}

	ChildSlot
	.Padding(FMargin(0.0f, 0.0f, AssetViewWidgetsShadowPadding, AssetViewWidgetsShadowPadding))
		[
			// Drop shadow border
			SNew(SBorder)
			.Padding(FMargin(0.0f, 0.0f, AssetViewWidgetsShadowPadding, AssetViewWidgetsShadowPadding))
			.BorderImage(FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.DropShadow"))
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.Padding(0)
					.BorderImage(this, &SMetaHumanCharacterAssetViewItem::GetNameAreaBackgroundImage)
					.ToolTipText(this, &SMetaHumanCharacterAssetViewItem::GetThumbnailName)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							// The remainder of the space is reserved for the name.
							SNew(SBox)
							.WidthOverride(ThumbnailSize)
							.HeightOverride(ThumbnailSize)
							.Padding(TileThumbnailPadding)
							[
								SNew(SOverlay)

								// The actual thumbnail
								+SOverlay::Slot()
								.Padding(1.f)
								[
									Thumbnail.ToSharedRef()
								]

								// Asset Color
								+SOverlay::Slot()
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Bottom)
								[
									SNew(SBorder)
									.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
									.BorderBackgroundColor(AssetColor)
									.Padding(FMargin(0.f, 2.f, 0.f, 0.f))
									.Visibility(this, &SMetaHumanCharacterAssetViewItem::GetAssetColorVisibility)
								]
	
								// Extra state
								+ SOverlay::Slot()
								.VAlign(VAlign_Bottom)
								.HAlign(HAlign_Left)
								[
									SNew(SBox)
									.MaxDesiredHeight(22.f)
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									.Padding(2.f, 0.f, 0.f, 4.f)
									[
										SNew(SBorder)
										.BorderImage(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.Rounded.WhiteBrush")))
										.BorderBackgroundColor(StateOverlayBorderColor)
										.HAlign(HAlign_Center)
										.VAlign(VAlign_Center)
										.Visibility(this, &SMetaHumanCharacterAssetViewItem::GetStatesOverlayBorderVisibility)
										[
											SNew(SHorizontalBox)

											// Dirty state icon
											+SHorizontalBox::Slot()
											.Padding(1.f, 0.f)
											.AutoWidth()
											[
												SNew(SBox)
												.WidthOverride(14.f)
												.HeightOverride(16.f)
												[
													SNew(SImage)
													.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.ContentDirty"))
													.Visibility(this, &SMetaHumanCharacterAssetViewItem::GetDirtyIconVisibility)
												]
											]

											// Supported state icon
											+ SHorizontalBox::Slot()
											.Padding(1.f, 0.f)
											.AutoWidth()
											[
												SNew(SBox)
												.WidthOverride(16.f)
												.HeightOverride(16.f)
												[
													SNew(SImage)
													.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.ContentSupported"))
													.Visibility(this, &SMetaHumanCharacterAssetViewItem::GetAvailableIconVisibility)
												]
											]

											// Checked state icon
											+ SHorizontalBox::Slot()
											.Padding(1.f, 0.f)
											.AutoWidth()
											[
												SNew(SBox)
												.WidthOverride(14.f)
												.HeightOverride(16.f)
												[
													SNew(SImage)
													.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.ContentChecked"))
													.Visibility(this, &SMetaHumanCharacterAssetViewItem::GetCheckedIconVisibility)
												]
											]

											// Active state icon
											+ SHorizontalBox::Slot()
											.Padding(1.f, 0.f)
											.AutoWidth()
											[
												SNew(SBox)
												.WidthOverride(16.f)
												.HeightOverride(16.f)
												[
													SNew(SImage)
													.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.ContentActive"))
													.Visibility(this, &SMetaHumanCharacterAssetViewItem::GetActiveIconVisibility)
												]
											]
										]
									]
								]
							]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBox)
							.Padding(AssetViewWidgetsNameWidgetPadding, AssetViewWidgetsNameWidgetPadding, 0.0f, 0.0f)
							.VAlign(VAlign_Top)
							.HAlign(HAlign_Left)
							.HeightOverride(TileTextSize)
							[
								SNew(STextBlock)
								.Font(FAppStyle::GetFontStyle("ContentBrowser.AssetTileViewNameFont"))
								.Text(this, &SMetaHumanCharacterAssetViewItem::GetThumbnailName)
								.OverflowPolicy(ETextOverflowPolicy::MultilineEllipsis)
								.ColorAndOpacity(this, &SMetaHumanCharacterAssetViewItem::GetNameAreaTextColor)
							]
						]
					]
				]
			]
		];
}

FReply SMetaHumanCharacterAssetViewItem::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		const FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, GenerateAssetItemContextMenu(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SMetaHumanCharacterAssetViewItem::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		bDraggedOver = true;
	}

	return FReply::Unhandled();
}

void SMetaHumanCharacterAssetViewItem::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bDraggedOver = false;
}

void SMetaHumanCharacterAssetViewItem::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	bDraggedOver = false;
}

FReply SMetaHumanCharacterAssetViewItem::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bDraggedOver = true;
	return FReply::Handled();
}

FReply SMetaHumanCharacterAssetViewItem::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (bDraggedOver)
	{
		bDraggedOver = false;
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SMetaHumanCharacterAssetViewItem::GenerateThumbnailWidget()
{
	constexpr FLinearColor ThumbnailBackgrounColor = FLinearColor(0.01f, 0.01f, 0.01f, 1.f);
	TSharedPtr<SWidget> ThumbnailWidget = 
		SNew(SBorder)
		.BorderImage(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.Rounded.WhiteBrush")))
		.BorderBackgroundColor(ThumbnailBackgrounColor)
		.Padding(-1.f, -1.f, -1.f, 0.f)
		[
			SNew(SImage)
			.Image(FAppStyle::GetDefaultBrush())
		];

	if (!AssetItem.IsValid() || !AssetThumbnail.IsValid())
	{
		return ThumbnailWidget.ToSharedRef();
	}

	OnOverrideThumbnailDelegate.ExecuteIfBound(AssetItem);

	if (AssetItem->ThumbnailImageOverride.IsValid() && AssetItem->AssetData.IsValid())
	{
		ThumbnailWidget =
			SNew(SOverlay)

			// Thumbnail image section
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.Rounded.WhiteBrush")))
				.BorderBackgroundColor(ThumbnailBackgrounColor)
				.Padding(-1.f, -1.f, -1.f, 0.f)
				[
					SNew(SImage)
					.Image(AssetItem->ThumbnailImageOverride->GetSlateBrush())
				]
			];
	}
	else
	{
		FAssetThumbnailConfig ThumbnailConfig;
		ThumbnailConfig.bAllowFadeIn = true;
		ThumbnailConfig.bAllowRealTimeOnHovered = false; // we use our own OnMouseEnter/Leave for logical asset item
		ThumbnailConfig.AllowAssetSpecificThumbnailOverlay = true;
		ThumbnailConfig.ShowAssetColor = true;
		ThumbnailConfig.AllowAssetStatusThumbnailOverlay = true;
		ThumbnailConfig.AssetBorderImageOverride = TAttribute<const FSlateBrush*>::CreateSP(this, &SMetaHumanCharacterAssetViewItem::GetAssetAreaOverlayBackgroundImage);

		ThumbnailWidget = 
			SNew(SBorder)
			.BorderImage(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.Rounded.WhiteBrush")))
			.BorderBackgroundColor(ThumbnailBackgrounColor)
			.Padding(-1.f, -1.f, -1.f, 0.f)
			[
				AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig)
			];

		// Use the same tooltip as the Thumbnail
		if (const TSharedPtr<IToolTip>& ThumbnailTooltip = ThumbnailWidget->GetToolTip())
		{
			SetToolTip(ThumbnailTooltip);
		}
	}

	return ThumbnailWidget.ToSharedRef();
}

TSharedRef<SWidget> SMetaHumanCharacterAssetViewItem::GenerateAssetItemContextMenu()
{
	constexpr bool bShouldCloseWindowAfterClosing = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

	MenuBuilder.BeginSection("OptionsSection", LOCTEXT("OptionsSection", "Options"));
	{
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("AssetViewItem_RemoveAsset_Label", "Remove Asset"),
			LOCTEXT("AssetViewItem_RemoveAsset_Tooltip", "Removes this asset"),
			FSlateIcon(),
			FUIAction
			(
				FExecuteAction::CreateLambda([this]()
					{
						OnDeletedDelegate.ExecuteIfBound(AssetItem);
					}),
				FCanExecuteAction::CreateLambda([this]()
					{
						if (CanDeleteDelegate.IsBound())
						{
							return CanDeleteDelegate.Execute(AssetItem);
						}

						return false;
					})
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("AssetViewsView_RemoveFolder_Label", "Remove Folder"),
			LOCTEXT("AssetViewsView_RemoveFolder_Tooltip", "Remove this folder from the Project Settings monitored paths"),
			FSlateIcon(),
			FUIAction
			(
				FExecuteAction::CreateLambda([this]()
					{
						OnDeletedFolderDelegate.ExecuteIfBound(AssetItem);
					}),
				FCanExecuteAction::CreateLambda([this]()
					{
						if (CanDeleteFolderDelegate.IsBound())
						{
							return CanDeleteFolderDelegate.Execute(AssetItem);
						}

						return false;
					})
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

const FSlateBrush* SMetaHumanCharacterAssetViewItem::GetAssetAreaOverlayBackgroundImage() const
{
	const bool bIsSelected = IsSelected.IsBound() ? IsSelected.Execute() : false;
	const bool bIsHoveredOrDraggedOver = IsHovered() || bDraggedOver || (Thumbnail.IsValid() && Thumbnail->IsHovered());
	if (bIsSelected && bIsHoveredOrDraggedOver)
	{
		static const FLazyName SelectedHover("ContentBrowser.AssetTileItem.AssetBorderSelectedHoverBackground");
		return FAppStyle::Get().GetBrush(SelectedHover);
	}
	else if (bIsSelected)
	{
		static const FLazyName Selected("ContentBrowser.AssetTileItem.AssetBorderSelectedBackground");
		return FAppStyle::Get().GetBrush(Selected);
	}
	else if (bIsHoveredOrDraggedOver)
	{
		static const FLazyName Hovered("ContentBrowser.AssetTileItem.AssetBorderHoverBackground");
		return FAppStyle::Get().GetBrush(Hovered);
	}
	else
	{
		static const FLazyName Normal("AssetThumbnail.AssetBorder");
		return FAppStyle::Get().GetBrush(Normal);
	}
}

const FSlateBrush* SMetaHumanCharacterAssetViewItem::GetNameAreaBackgroundImage() const
{
	const FName SelectedHover = "ContentBrowser.AssetTileItem.AssetContentSelectedHoverBackground";
	const FName Selected = "ContentBrowser.AssetTileItem.AssetContentSelectedBackground";
	const FName Hovered = "ContentBrowser.AssetTileItem.AssetContentHoverBackground";
	const FName Normal = "ContentBrowser.AssetTileItem.AssetContent";

	const bool bIsSelected = IsSelected.IsBound() ? IsSelected.Execute() : false;
	const bool bIsHoveredOrDraggedOver = IsHovered() || bDraggedOver || (Thumbnail.IsValid() && Thumbnail->IsHovered());
	if (bIsSelected && bIsHoveredOrDraggedOver)
	{
		return FAppStyle::Get().GetBrush(SelectedHover);
	}
	else if (bIsSelected)
	{
		return FAppStyle::Get().GetBrush(Selected);
	}
	else if (bIsHoveredOrDraggedOver)
	{
		return FAppStyle::Get().GetBrush(Hovered);
	}
	else
	{
		return FAppStyle::Get().GetBrush(Normal);
	}
}

FSlateColor SMetaHumanCharacterAssetViewItem::GetNameAreaTextColor() const
{
	const bool bIsSelected = IsSelected.IsBound() ? IsSelected.Execute() : false;
	const bool bIsHoveredOrDraggedOver = IsHovered() || bDraggedOver || (Thumbnail.IsValid() && Thumbnail->IsHovered());
	if (bIsSelected || bIsHoveredOrDraggedOver)
	{
		return FStyleColors::White;
	}

	return FSlateColor::UseForeground();
}

FText SMetaHumanCharacterAssetViewItem::GetThumbnailName() const
{
	FText ThumbnailName = FText::FromName(AssetItem->AssetData.AssetName);
	if (OnOverrideThumbnailNameDelegate.IsBound())
	{
		FText NameFromProperty = OnOverrideThumbnailNameDelegate.Execute(AssetItem);
		if (!NameFromProperty.IsEmpty())
		{
			ThumbnailName = NameFromProperty;
		}
	}

	return ThumbnailName;
}

bool SMetaHumanCharacterAssetViewItem::IsItemDirty() const
{
	TArray<UPackage*> DirtyPackages;
	UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyPackages);

	for (UPackage* Package : DirtyPackages)
	{
		if (Package && Package->GetName() == AssetItem->AssetData.PackageName)
		{
			return Package->IsDirty();
		}
	}
	
	return false;
}

bool SMetaHumanCharacterAssetViewItem::IsItemChecked() const
{
	return IsChecked.IsBound() && IsChecked.Execute(AssetItem);
}

bool SMetaHumanCharacterAssetViewItem::IsItemAvailable() const
{
	bool bIsAvailable = true;
	if (IsAvailable.IsBound())
	{
		bIsAvailable = IsAvailable.Execute(AssetItem);
	}

	return bIsAvailable;
}

bool SMetaHumanCharacterAssetViewItem::IsItemActive() const
{
	return IsActive.IsBound() && IsActive.Execute(AssetItem);
}

EVisibility SMetaHumanCharacterAssetViewItem::GetAssetColorVisibility() const
{
	const bool bIsVisible =
		OnOverrideThumbnailDelegate.IsBound() &&
		AssetItem.IsValid() &&
		AssetItem->ThumbnailImageOverride.IsValid();
	
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMetaHumanCharacterAssetViewItem::GetDirtyIconVisibility() const
{
	return IsItemDirty() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMetaHumanCharacterAssetViewItem::GetCheckedIconVisibility() const
{
	return IsItemChecked() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMetaHumanCharacterAssetViewItem::GetAvailableIconVisibility() const
{
	return IsItemAvailable() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SMetaHumanCharacterAssetViewItem::GetActiveIconVisibility() const
{
	return IsItemActive() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMetaHumanCharacterAssetViewItem::GetStatesOverlayBorderVisibility() const
{
	const bool bIsVisible =
		IsItemDirty() ||
		IsItemChecked() ||
		!IsItemAvailable() ||
		IsItemActive();

	return bIsVisible ? EVisibility::Visible : EVisibility::Hidden;
}

SMetaHumanCharacterEditorAssetsView::SMetaHumanCharacterEditorAssetsView()
	: ListItems(MakeShared<UE::Slate::Containers::TObservableArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>>>())
{
}

SMetaHumanCharacterEditorAssetsView::~SMetaHumanCharacterEditorAssetsView()
{
	for (const FMetaHumanCharacterAssetsSection& Section : Sections)
	{
		FMetaHumanCharacterAssetObserver::Get().UnsubscribeFromObserver(Section.SlotName, SubscriberHandle);
	}
}

void SMetaHumanCharacterEditorAssetsView::Construct(const FArguments& InArgs)
{
	using namespace UE::MetaHuman::Private;

	Sections = InArgs._Sections;
	SelectionMode = InArgs._SelectionMode;
	ExcludedObjects = InArgs._ExcludedObjects;
	SlotName = InArgs._SlotName;
	Label = InArgs._Label;
	MaxHeight = InArgs._MaxHeight;

	bAutoHeight = InArgs._AutoHeight;
	bAllowDragging = InArgs._AllowDragging;
	bAllowDropping = InArgs._AllowDropping;
	bHasVirtualFolder = InArgs._HasVirtualFolder;

	OnOverrideThumbnailDelegate = InArgs._OnOverrideThumbnail;
	OnOverrideThumbnailNameDelegate = InArgs._OnOverrideThumbnailName;
	OnProcessDroppedItemDelegate = InArgs._OnProcessDroppedItem;
	OnProcessDroppedFoldersDelegate = InArgs._OnProcessDroppedFolders;
	OnPopulateItemsDelegate = InArgs._OnPopulateItems;
	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;
	OnItemActivatedDelegate = InArgs._OnItemActivated;
	OnItemDeletedDelegate = InArgs._OnItemDeleted;
	CanDeleteItemDelegate = InArgs._CanDeleteItem;
	OnFolderDeletedDelegate = InArgs._OnFolderDeleted;
	CanDeleteFolderDelegate = InArgs._CanDeleteFolder;
	OnHandleVirtualItemDelegate = InArgs._OnHadleVirtualItem;

	IsItemCompatible = InArgs._IsItemCompatible;
	IsItemChecked = InArgs._IsItemChecked;
	IsItemAvailable = InArgs._IsItemAvailable;
	IsItemActive = InArgs._IsItemActive;

	AssetThumbnailPool = MakeShared<FAssetThumbnailPool>(ThumbnailPoolSize);

	for (const FMetaHumanCharacterAssetsSection& Section : Sections)
	{
		if (Section.bPureVirtual)
		{
			continue;
		}

		FMetaHumanCharacterAssetObserver::Get().StartObserving(FName(Section.ContentDirectoryToMonitor.Path));
		SubscriberHandle = FMetaHumanCharacterAssetObserver::Get().SubscribeToObserver(
			FName(Section.ContentDirectoryToMonitor.Path),
			FOnObservedDirectoryChanged::CreateSPLambda(this,
				[this](const FMetaHumanObserverChanges& InChanges)
				{
					PopulateListItems(InChanges);
				}));
	}

	TSharedPtr<SVerticalBox> ContainerBox;

	ChildSlot
		[
			SAssignNew(ContainerBox, SVerticalBox)

			+SVerticalBox::Slot()
			.MinHeight(40.f)
			[
				SNew(SBox)
				[
					SAssignNew(TileView, STileView<TSharedPtr<FMetaHumanCharacterAssetViewItem>>)
					.SelectionMode(SelectionMode)
					.ListItemsSource(ListItems)
					.ItemWidth(TileSize)
					.ItemHeight(TileSize + TileTextSize)
					.ItemAlignment(EListItemAlignment::LeftAligned)
					.OnGenerateTile(this, &SMetaHumanCharacterEditorAssetsView::OnGenerateTile)
					.OnMouseButtonDoubleClick(OnItemActivatedDelegate)
					.OnSelectionChanged(OnSelectionChangedDelegate)
					.Visibility_Lambda([this, InArgs]()
						{
							const bool bIsVisible =
								InArgs._Visibility.IsBound() &&
								InArgs._Visibility.Get() == EVisibility::Visible &&
								!ListItems.Get().IsEmpty();

							return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
						})
				]
			]

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.Padding(4.f)
			.AutoHeight()
			[
				SNew(SBox)
				.MinDesiredHeight(40.f)
				.Visibility(this, &SMetaHumanCharacterEditorAssetsView::GetDroppingAreaVisibility)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(FText::FromString("Drag a Folder or compatible Asset here."))
					.Font(IDetailLayoutBuilder::GetDetailFontItalic())
					.ColorAndOpacity(FStyleColors::AccentGray)
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
				]
			]
		];

	if (ContainerBox.IsValid())
	{
		SVerticalBox::FSlot& Slot = ContainerBox->GetSlot(0);
		if (bAutoHeight)
		{
			Slot.SetAutoHeight();
		}
		else
		{
			Slot.SetMaxHeight(MaxHeight);
		}
	}

	PopulateListItems({});
}

float SMetaHumanCharacterEditorAssetsView::GetScrollOffset() const
{
	return TileView.IsValid() ? TileView->GetScrollOffset() : 0.f;
}

void SMetaHumanCharacterEditorAssetsView::PopulateListItems(const FMetaHumanObserverChanges& InChanges)
{
	TArray<FMetaHumanCharacterAssetViewItem> Items;
	if (OnPopulateItemsDelegate.IsBound())
	{
		for (const FMetaHumanCharacterAssetsSection& Section : Sections)
		{
			Items.Append(OnPopulateItemsDelegate.Execute(Section, InChanges));
		}
	}

	ListItems->Reset(Items.Num());
	for (const FMetaHumanCharacterAssetViewItem& Item : Items)
	{
		if (Item.AssetData.PackageName == FName(*GetTransientPackage()->GetName()))
		{
			continue;
		}

		// Avoid item duplication
		const FSoftObjectPath AssetPath = Item.AssetData.GetSoftObjectPath();
		const bool bIsAlreadyContained = ListItems->ContainsByPredicate(
			[AssetPath](const TSharedPtr<FMetaHumanCharacterAssetViewItem>& ItemPtr)
			{
				return ItemPtr.IsValid() && ItemPtr->AssetData.GetSoftObjectPath() == AssetPath;
			}) > 0;

		if (IsAssetFiltered(Item.AssetData) && !bIsAlreadyContained)
		{
			ListItems->Add(MakeShared<FMetaHumanCharacterAssetViewItem>(Item.AssetData, Item.SlotName, Item.PaletteItemKey, AssetThumbnailPool));
		}
	}
}

void SMetaHumanCharacterEditorAssetsView::SetScrollOffset(float InScrollOffset)
{
	if (TileView.IsValid())
	{
		TileView->SetScrollOffset(InScrollOffset);
	}
}

void SMetaHumanCharacterEditorAssetsView::SetFilter(const FText& InNewSearchText)
{
	if (InNewSearchText.IsEmptyOrWhitespace())
	{
		SearchText.Empty();
	}
	else
	{
		SearchText = InNewSearchText.ToString();
	}

	PopulateListItems({});
}

void SMetaHumanCharacterEditorAssetsView::SetItemSelection(TSharedPtr<FMetaHumanCharacterAssetViewItem> InItem, bool bSelected, ESelectInfo::Type SelectInfo)
{
	if (InItem.IsValid() && TileView.IsValid())
	{
		TileView->SetItemSelection(InItem, bSelected, SelectInfo);
	}
}

void SMetaHumanCharacterEditorAssetsView::ClearSelection()
{
	if (TileView.IsValid())
	{
		TileView->ClearSelection();
	}
}

TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> SMetaHumanCharacterEditorAssetsView::GetSelectedItems() const
{
	return TileView->GetSelectedItems();
}

TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> SMetaHumanCharacterEditorAssetsView::GetItems() const
{
	TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> Items;
	if (TileView.IsValid())
	{
		Items = TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>>(TileView->GetItems().GetData(), TileView->GetItems().Num());
	}

	return Items;
}

void SMetaHumanCharacterEditorAssetsView::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bDraggedOver = true;
}

void SMetaHumanCharacterEditorAssetsView::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	bDraggedOver = false;
}
FReply SMetaHumanCharacterEditorAssetsView::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (!bDraggedOver)
	{
		bDraggedOver = true;
	}

	return FReply::Unhandled();
}

FReply SMetaHumanCharacterEditorAssetsView::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bDraggedOver = false;
	if (Sections.IsEmpty() || !bAllowDropping || DragDropEvent.GetOperationAs<FMetaHumanCharacterAssetViewItemDragDropOp>().IsValid())
	{
		return FReply::Handled();
	}

	TArray<FAssetData> AssetDataArray;
	const TSharedPtr<FContentBrowserDataDragDropOp> ContentBrowserDragDropOperation = DragDropEvent.GetOperationAs<FContentBrowserDataDragDropOp>();
	if (ContentBrowserDragDropOperation.IsValid())
	{
		AssetDataArray.Append(ContentBrowserDragDropOperation->GetAssets());

		const TArray<FContentBrowserItem>& DroppedFolders = ContentBrowserDragDropOperation->GetDraggedFolders();
		OnProcessDroppedFoldersDelegate.ExecuteIfBound(DroppedFolders, Sections[0]);
	}
	else
	{
		return FReply::Handled();
	}

	if (AssetDataArray.IsEmpty())
	{
		return FReply::Handled();
	}

	// Create transaction for items added in the virtual folder
	TUniquePtr<FScopedTransaction> Transaction;
	for (const FAssetData& AssetData : AssetDataArray)
	{
		if (!AssetData.IsValid())
		{
			continue;
		}

		const FString AssetPath = AssetData.PackagePath.ToString();
		UObject* AssetObject = AssetData.GetAsset();
		FAssetData ProcessedAssetData = AssetData;
		if (OnProcessDroppedItemDelegate.IsBound())
		{
			AssetObject = OnProcessDroppedItemDelegate.Execute(AssetData);
			ProcessedAssetData = FAssetData(AssetObject);
		}

		if (!AssetObject || ExcludedObjects.Contains(AssetObject))
		{
			continue;
		}

		bool bIsCompatible = false;
		if (IsItemCompatible.IsBound())
		{
			bIsCompatible = Algo::AnyOf(Sections, [this, ProcessedAssetData](const FMetaHumanCharacterAssetsSection& Section)
				{
					const TSharedRef<FMetaHumanCharacterAssetViewItem> NewAssetItem = MakeShared<FMetaHumanCharacterAssetViewItem>(ProcessedAssetData, Section.SlotName, FMetaHumanPaletteItemKey(), AssetThumbnailPool);
					return IsItemCompatible.Execute(NewAssetItem, Section);
				});
		}
		else
		{
			bIsCompatible = Algo::AnyOf(Sections, [AssetObject](const FMetaHumanCharacterAssetsSection& Section)
				{
					return Section.ClassesToFilter.Contains(AssetObject->GetClass());
				});
		}

		if (!bIsCompatible)
		{
			continue;
		}

		bool bIsAlreadyContained = ListItems->ContainsByPredicate([ProcessedAssetData](const TSharedPtr<FMetaHumanCharacterAssetViewItem>& Item)
			{
				return Item.IsValid() && Item->AssetData.ToSoftObjectPath() == ProcessedAssetData.ToSoftObjectPath();
			}) > 0;

		if(bIsAlreadyContained)
		{ 
			continue;
		}

		if (bHasVirtualFolder)
		{
			if (!Transaction.IsValid())
			{
				Transaction = MakeUnique<FScopedTransaction>(LOCTEXT("MetaHumanCharacter_AddAssetViewItems", "Add Items"));
			}

			const UObject* SavedObject = SaveAssetToSectionFolder(ProcessedAssetData, AssetPath);
			if (SavedObject)
			{
				ProcessedAssetData = FAssetData(SavedObject);
				HandleVirtualFolderAsset(ProcessedAssetData);
			}
		}
		else if (Sections.Num() == 1)
		{
			constexpr bool bAllowMoving = true;
			SaveAssetToSectionFolder(ProcessedAssetData, Sections[0].ContentDirectoryToMonitor.Path, bAllowMoving);
		}
	}

	PopulateListItems({});
	return FReply::Handled();
}

bool SMetaHumanCharacterEditorAssetsView::IsAssetFiltered(const FAssetData& InAssetData) const
{
	bool bIsFiltered = true;
	if (!SearchText.IsEmpty())
	{
		bIsFiltered &= InAssetData.AssetName.ToString().Contains(SearchText);
	}

	if (!ExcludedObjects.IsEmpty())
	{
		bIsFiltered &= !ExcludedObjects.ContainsByPredicate(
			[InAssetData](const TWeakObjectPtr<UObject>& Object)
			{
				return Object.IsValid() && FSoftObjectPath(Object.Get()) == InAssetData.GetSoftObjectPath();
			});
	}

	return bIsFiltered;
}

TSharedRef<ITableRow> SMetaHumanCharacterEditorAssetsView::OnGenerateTile(TSharedPtr<FMetaHumanCharacterAssetViewItem> InItem, const TSharedRef<STableViewBase>& OwnerTable) const
{
	using namespace UE::MetaHuman::Private;

	if (!ensure(InItem.IsValid()))
	{
		return SNew(STableRow<TSharedPtr<FMetaHumanCharacterAssetViewItem>>, OwnerTable);
	}
	
	const TSharedRef<FAssetThumbnail> AssetThumbnail = InItem->Thumbnail.ToSharedRef();
	AssetThumbnail->GetViewportRenderTargetTexture();

	TSharedPtr<STableRow<TSharedPtr<FMetaHumanCharacterAssetViewItem>>> TableRowWidget;
	SAssignNew(TableRowWidget, STableRow<TSharedPtr<FMetaHumanCharacterAssetViewItem>>, OwnerTable)
		.Style(FMetaHumanCharacterEditorStyle::Get(), "MetaHumanCharacterEditorTools.AssetView")
		.Padding(4.f)
		.Cursor(bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default)
		.OnDragDetected(this, &SMetaHumanCharacterEditorAssetsView::OnDraggingAssetItem);

	TSharedRef<SMetaHumanCharacterAssetViewItem> Item =
		SNew(SMetaHumanCharacterAssetViewItem)
		.AssetItem(InItem)
		.AssetThumbnail(AssetThumbnail)
		.IsSelected(FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FMetaHumanCharacterAssetViewItem>>::IsSelected))
		.IsChecked(IsItemChecked)
		.IsAvailable(IsItemAvailable)
		.IsActive(IsItemActive)
		.OnOverrideThumbnail(OnOverrideThumbnailDelegate)
		.OnOverrideThumbnailName(OnOverrideThumbnailNameDelegate)
		.OnDeleted(OnItemDeletedDelegate)
		.CanDelete(CanDeleteItemDelegate)
		.OnDeletedFolder(this, &SMetaHumanCharacterEditorAssetsView::OnDeletedFolder)
		.CanDeleteFolder(this, &SMetaHumanCharacterEditorAssetsView::CanDeleteFolder);
	
	TableRowWidget->SetContent(Item);
	return TableRowWidget.ToSharedRef();
}

void SMetaHumanCharacterEditorAssetsView::OnDeletedFolder(TSharedPtr<FMetaHumanCharacterAssetViewItem> InItem) const
{
	if (InItem.IsValid() && Sections.Num() == 1 && !Sections[0].bPureVirtual)
	{
		OnFolderDeletedDelegate.ExecuteIfBound(Sections[0]);
	}
}

bool SMetaHumanCharacterEditorAssetsView::CanDeleteFolder(TSharedPtr<FMetaHumanCharacterAssetViewItem> InItem) const
{
	if (!InItem.IsValid() || Sections.Num() != 1 || Sections[0].bPureVirtual || !CanDeleteFolderDelegate.IsBound())
	{
		return false;
	}

	return CanDeleteFolderDelegate.Execute(InItem, Sections[0]);
}

FReply SMetaHumanCharacterEditorAssetsView::OnDraggingAssetItem(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const 
{
	if (bAllowDragging && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> SelectedItems = GetSelectedItems();
		if (SelectedItems.Num() > 0)
		{
			const TSharedPtr<FMetaHumanCharacterAssetViewItem> FirstSelectedItem = SelectedItems[0];
			const FAssetData& AssetData = FirstSelectedItem->AssetData;

			const TSharedRef<FAssetDragDropOp> DragDropOp = FMetaHumanCharacterAssetViewItemDragDropOp::New(AssetData, nullptr, FirstSelectedItem);
			return FReply::Handled().BeginDragDrop(DragDropOp);
		}
	}

	return FReply::Unhandled();
}

EVisibility SMetaHumanCharacterEditorAssetsView::GetDroppingAreaVisibility() const
{
	return bAllowDropping ? EVisibility::Visible : EVisibility::Collapsed;
}

void SMetaHumanCharacterEditorAssetsView::HandleVirtualFolderAsset(const FAssetData& AssetData)
{
	using namespace UE::MetaHuman::Private;

	if (!AssetData.IsValid())
	{
		return;
	}

	if (ListItems->ContainsByPredicate([AssetData](const TSharedPtr<FMetaHumanCharacterAssetViewItem>& Item)
		{
			return Item.IsValid() && Item->AssetData.ToSoftObjectPath() == AssetData.ToSoftObjectPath();
		}) > 0)
	{
		return;
	}

	FMetaHumanPaletteItemKey PaletteKey;
	if (AssetData.IsAssetLoaded())
	{
		PaletteKey = FMetaHumanPaletteItemKey(TNotNull<UObject*>(AssetData.GetAsset()), NAME_None);
	}

	const TSharedRef<FMetaHumanCharacterAssetViewItem> NewAssetItem = MakeShared<FMetaHumanCharacterAssetViewItem>(AssetData, SlotName, PaletteKey, AssetThumbnailPool);
	OnHandleVirtualItemDelegate.ExecuteIfBound(NewAssetItem);
}

UObject* SMetaHumanCharacterEditorAssetsView::SaveAssetToSectionFolder(const FAssetData& AssetData, const FString& FolderPath, bool bAllowMoving)
{
	UObject* AssetObject = AssetData.GetAsset();
	FString LocalFolderPath;
	if (!AssetObject || !FPackageName::TryConvertGameRelativePackagePathToLocalPath(FolderPath, LocalFolderPath))
	{
		return nullptr;
	}

	if (!IFileManager::Get().DirectoryExists(*LocalFolderPath))
	{
		return nullptr;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	const UPackage* TransientPackage = GetTransientPackage();
	const FString AssetName = AssetObject->GetName();

	if (AssetObject->GetPackage() == TransientPackage)
	{
		AssetObject->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		AssetObject = AssetTools.DuplicateAsset(AssetName, FolderPath, AssetObject);
	}
	else if (bAllowMoving)
	{
		const FAssetRenameData RenameData(AssetObject, FolderPath, AssetName);
		AssetTools.RenameAssets({ RenameData });
	}

	return AssetObject;
}

SMetaHumanCharacterEditorAssetViewsPanel::~SMetaHumanCharacterEditorAssetViewsPanel()
{
	const FName MenuName = GetSettingsMenuName();

	UToolMenus* ToolMenus = UToolMenus::Get();
	if (ToolMenus->IsMenuRegistered(MenuName))
	{
		ToolMenus->RemoveMenu(MenuName);
	}
}

void SMetaHumanCharacterEditorAssetViewsPanel::Construct(const FArguments& InArgs)
{
	AssetViewSections = InArgs._AssetViewSections;
	ExcludedObjects = InArgs._ExcludedObjects;
	VirtualFolderClassesToFilter = InArgs._VirtualFolderClassesToFilter;

	bAutoHeight = InArgs._AutoHeight;
	bAllowDragging = InArgs._AllowDragging;
	bAllowSlots = InArgs._AllowSlots;
	bAllowMultiSelection = InArgs._AllowMultiSelection;
	bAllowSlotMultiSelection = InArgs._AllowSlotMultiSelection;

	OnOverrideSlotNameDelegate = InArgs._OnOverrideSlotName;
	OnOverrideThumbnailDelegate = InArgs._OnOverrideThumbnail;
	OnOverrideThumbnailNameDelegate = InArgs._OnOverrideThumbnailName;
	OnProcessDroppedItemDelegate = InArgs._OnProcessDroppedItem;
	OnProcessDroppedFoldersDelegate = InArgs._OnProcessDroppedFolders;
	OnPopulateAssetViewsItemsDelegate = InArgs._OnPopulateAssetViewsItems;
	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;
	OnItemActivatedDelegate = InArgs._OnItemActivated;
	OnItemDeletedDelegate = InArgs._OnItemDeleted;
	CanDeleteItemDelegate = InArgs._CanDeleteItem;
	OnFolderDeletedDelegate = InArgs._OnFolderDeleted;
	CanDeleteFolderDelegate = InArgs._CanDeleteFolder;
	OnHandleVirtualItemDelegate = InArgs._OnHadleVirtualItem;

	IsItemCompatible = InArgs._IsItemCompatible;
	IsItemChecked = InArgs._IsItemChecked;
	IsItemAvailable = InArgs._IsItemAvailable;
	IsItemActive = InArgs._IsItemActive;

	ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// Search Box section
				+SHorizontalBox::Slot()
				.Padding(2.f, 0.f)
				[
					SAssignNew(SearchBox, SSearchBox)
					.OnTextChanged(this, &SMetaHumanCharacterEditorAssetViewsPanel::OnSearchBoxTextChanged)
				]

				// Settings button section
				+ SHorizontalBox::Slot()
				.Padding(2.f, 0.f)
				.AutoWidth()
				[
					SNew(SComboButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.HasDownArrow(true)
					.OnGetMenuContent(this, &SMetaHumanCharacterEditorAssetViewsPanel::GenerateSettingsMenuWidget)
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
					]
				]
			]

			// Asset Views section
			+ SVerticalBox::Slot()
			.Padding(2.f, 6.f)
			.AutoHeight()
			[
				SAssignNew(AssetViewSlotsBox, SVerticalBox)
			]
		];

	MakeAssetViewsPanel();
}

TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> SMetaHumanCharacterEditorAssetViewsPanel::GetSelectedItems() const
{
	TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> SelectedItems;
	for (const TSharedPtr<SMetaHumanCharacterEditorAssetsView>& AssetView : AssetViews)
	{
		if (!AssetView.IsValid())
		{
			continue;
		}

		TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> Items = AssetView->GetSelectedItems();
		if (Items.IsEmpty())
		{
			continue;
		}

		SelectedItems.Append(Items);
	}

	return SelectedItems;
}

TSharedPtr<SMetaHumanCharacterEditorAssetsView> SMetaHumanCharacterEditorAssetViewsPanel::GetOwnerAssetView(TSharedPtr<FMetaHumanCharacterAssetViewItem> InSelectedItem) const
{
	for (const TSharedPtr<SMetaHumanCharacterEditorAssetsView>& AssetView : AssetViews)
	{
		if (!AssetView.IsValid())
		{
			continue;
		}

		const TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> Items = AssetView->GetItems();
		if (Items.Contains(InSelectedItem))
		{
			return AssetView;
		}
	}

	return nullptr;
}

void SMetaHumanCharacterEditorAssetViewsPanel::RequestRefresh()
{
	if (!RefreshAssetViewsTimerHandle.IsValid())
	{
		RefreshAssetViewsTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SMetaHumanCharacterEditorAssetViewsPanel::Refresh));
	}
}

FReply SMetaHumanCharacterEditorAssetViewsPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::Delete)
	{
		return FReply::Handled();
	}

	const TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> SelectedItems = GetSelectedItems();
	if (SelectedItems.IsEmpty())
	{
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("MetaHumanCharacter_DeleteAssetViewItems", "Delete Items"));
	for (const TSharedPtr<FMetaHumanCharacterAssetViewItem>& SelectedItem : SelectedItems)
	{
		if (SelectedItem.IsValid())
		{
			OnItemDeletedDelegate.ExecuteIfBound(SelectedItem);
		}
	}

	RequestRefresh();
	return FReply::Handled();
}

void SMetaHumanCharacterEditorAssetViewsPanel::PostUndo(bool bSuccess)
{
	RequestRefresh();
}

void SMetaHumanCharacterEditorAssetViewsPanel::PostRedo(bool bSuccess)
{
	RequestRefresh();
}

void SMetaHumanCharacterEditorAssetViewsPanel::MakeAssetViewsPanel()
{
	if (!AssetViewSlotsBox.IsValid())
	{
		return;
	}

	AssetViewSlotsBox->ClearChildren();
	AssetViews.Empty();

	PopulateSlotNames();

	for (const FName& SlotName : AssetViewsSlotNames)
	{
		AssetViewSlotsBox->AddSlot()
			.AutoHeight()
			[
				GenerateAssetViewsSlot(SlotName)
			];
	}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorAssetViewsPanel::GenerateAssetViewsSlot(const FName& SlotName)
{
	using namespace UE::MetaHuman::Private;

	const FName FullSlotName = OnOverrideSlotNameDelegate.IsBound() ? OnOverrideSlotNameDelegate.Execute(SlotName) : *GenericSlotText.ToString();
	TArray<FMetaHumanCharacterAssetsSection> Sections = GetSectionsBySlotName(SlotName);

	// Add individual assets section
	FMetaHumanCharacterAssetsSection IndividualAssetsSection;
	IndividualAssetsSection.SlotName = SlotName;
	IndividualAssetsSection.ContentDirectoryToMonitor.Path = VirtualFolderText.ToString();
	IndividualAssetsSection.ClassesToFilter = VirtualFolderClassesToFilter;
	IndividualAssetsSection.bPureVirtual = true;
	Sections.Add(IndividualAssetsSection);

	const TSharedRef<SVerticalBox> AssetViewsBox = SNew(SVerticalBox);
	TSharedPtr<SWidget> AssetViewsSlotWidget = AssetViewsBox;
	if (bAllowSlots)
	{
		SAssignNew(AssetViewsSlotWidget, SMetaHumanCharacterEditorToolPanel)
			.Label(FText::FromName(FullSlotName != NAME_None ? FullSlotName : *GenericSlotText.ToString()))
			.HierarchyLevel(EMetaHumanCharacterEditorPanelHierarchyLevel::Top)
			.IconBrush(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.AssetViewSlot")))
			.RoundedBorders(false)
			.HeaderContent()
			[
				GenerateSectionToolbar()
			]
			.Content()
			[
				AssetViewsBox
			];
	}

	const bool bShowSingleFolder =
		!bShowFolders ||
		Algo::AllOf(Sections, 
			[](const FMetaHumanCharacterAssetsSection& Section)
			{
				return Section.bPureVirtual;
			});

	if (bShowSingleFolder)
	{
		const bool bHasVirtualFolder = true;
		// Add a single asset view for all sections
		AssetViewsBox->AddSlot()
			.Padding(-6.f, -4.f)
			.AutoHeight()
			[
				SNew(SMetaHumanCharacterEditorToolPanel)
				.HierarchyLevel(EMetaHumanCharacterEditorPanelHierarchyLevel::Middle)
				.Label(MultiFolderText)
				.RoundedBorders(!bAllowSlots)
				.Padding(8.f)
				.HeaderContent()
				[
					GenerateSectionToolbar()
				]
				.Content()
				[
					GenerateAssetView(Sections, SlotName, bHasVirtualFolder)
				]
			];
	}
	else
	{
		// Gather all sections before iterating
		for (const FMetaHumanCharacterAssetsSection& Section : Sections)
		{
			const bool bHasVirtualFolder = Section.bPureVirtual;
			const FString FolderName = Section.ContentDirectoryToMonitor.Path;
			const FSlateBrush* FolderIcon = bHasVirtualFolder ? nullptr : FAppStyle::Get().GetBrush(TEXT("Icons.FolderClosed"));

			AssetViewsBox->AddSlot()
				.Padding(-6.f, -4.f)
				.AutoHeight()
				[
					SNew(SMetaHumanCharacterEditorToolPanel)
					.HierarchyLevel(EMetaHumanCharacterEditorPanelHierarchyLevel::Middle)
					.IconBrush(FolderIcon)
					.Label(FText::FromString(FolderName))
					.RoundedBorders(!bAllowSlots)
					.Padding(8.f)
					.HeaderContent()
					[
						GenerateSectionToolbar()
					]
					.Content()
					[
						GenerateAssetView({ Section }, SlotName, bHasVirtualFolder)
					]
				];
		}
	}

	return AssetViewsSlotWidget.ToSharedRef();
}

TSharedRef<SWidget> SMetaHumanCharacterEditorAssetViewsPanel::GenerateAssetView(const TArray<FMetaHumanCharacterAssetsSection>& Sections, const FName& SlotName, bool bHasVirtualFolder)
{
	const FString Label = GenerateAssetViewNameLabel(Sections, SlotName, bHasVirtualFolder);

	const TSharedRef<SMetaHumanCharacterEditorArrowButton> ArrowButton = SNew(SMetaHumanCharacterEditorArrowButton);
	const TSharedRef<SMetaHumanCharacterEditorAssetsView> NewAssetView =
		SNew(SMetaHumanCharacterEditorAssetsView)
		.Label(Label)
		.SlotName(SlotName)
		.Sections(Sections)
		.ExcludedObjects(ExcludedObjects)
		.SelectionMode(bAllowMultiSelection ? ESelectionMode::Multi : ESelectionMode::Single)
		.AutoHeight(bAutoHeight)
		.AllowDragging(bAllowDragging)
		.AllowDropping(bHasVirtualFolder)
		.HasVirtualFolder(bHasVirtualFolder)
		.OnOverrideThumbnail(OnOverrideThumbnailDelegate)
		.OnOverrideThumbnailName(OnOverrideThumbnailNameDelegate)
		.OnProcessDroppedItem(OnProcessDroppedItemDelegate)
		.OnProcessDroppedFolders(OnProcessDroppedFoldersDelegate)
		.IsItemCompatible(IsItemCompatible)
		.IsItemChecked(IsItemChecked)
		.IsItemAvailable(IsItemAvailable)
		.IsItemActive(IsItemActive)
		.OnPopulateItems(OnPopulateAssetViewsItemsDelegate)
		.OnSelectionChanged(this, &SMetaHumanCharacterEditorAssetViewsPanel::OnItemSelectionChanged)
		.OnItemActivated(OnItemActivatedDelegate)
		.OnItemDeleted(OnItemDeletedDelegate)
		.CanDeleteItem(CanDeleteItemDelegate)
		.OnHadleVirtualItem(OnHandleVirtualItemDelegate)
		.OnFolderDeleted(OnFolderDeletedDelegate)
		.CanDeleteFolder(CanDeleteFolderDelegate)
		.Visibility_Lambda([ArrowButton]()
			{
				return ArrowButton->IsExpanded() ? EVisibility::Visible : EVisibility::Collapsed;
			});

	AssetViews.Add(NewAssetView);
	return NewAssetView;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorAssetViewsPanel::GenerateSectionToolbar() const
{
	const TSharedRef<SWidget> ToolbarWidget =
		SNew(SHorizontalBox)
		/*
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			[
				SNew(SImage)
				.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.LoadedLayer")))
			]
		]*/;

	return ToolbarWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorAssetViewsPanel::GenerateSettingsMenuWidget()
{
	const FName MenuName = GetSettingsMenuName();

	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(MenuName);
		FToolMenuSection& Section = Menu->AddSection("OptionsSection", LOCTEXT("OptionsSection", "Options"));

		Section.AddMenuEntry
		(
			TEXT("Show Folders"),
			LOCTEXT("AssetViewsPanel_ShowFoldersOption_Label", "Show Folders"),
			LOCTEXT("AssetViewsPanel_ShowFolders_Tooltip", "Toggle showing assets under their containing folders, or pooled and sorted alphabetically"),
			FSlateIcon(),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorAssetViewsPanel::ToggleShowFolders),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return bShowFolders; })
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry
		(
			TEXT("Refresh Thumbnails"),
			LOCTEXT("AssetViewsPanel_Refresh Thumbnails_Label", "Refresh Thumbnails"),
			LOCTEXT("AssetViewsPanel_Refresh Thumbnails_Tooltip", "Refreshes Thumbnails for all assets"),
			FSlateIcon(),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorAssetViewsPanel::RequestRefresh)
			),
			EUserInterfaceActionType::Button
		);

		Section.AddMenuEntry
		(
			TEXT("Open Project Settings"),
			LOCTEXT("AssetViewsPanel_OpenProjectSettingsOption_Label", "Open Project Settings"),
			LOCTEXT("AssetViewsPanel_OpenProjectSettingsOption_Tooltip", "Configure Project Settings to edit or removed what folders are being monitored for content"),
			FSlateIcon(),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SMetaHumanCharacterEditorAssetViewsPanel::OpenProjectSettings)
			),
			EUserInterfaceActionType::Button
		);
	}

	FToolMenuContext Context;
	UToolMenu* Menu = ToolMenus->GenerateMenu(MenuName, Context);

	return ToolMenus->GenerateWidget(Menu);
}

FString SMetaHumanCharacterEditorAssetViewsPanel::GenerateAssetViewNameLabel(const TArray<FMetaHumanCharacterAssetsSection>& Sections, const FName& SlotName, bool bHasVirtualFolder) const
{
	FString NameLabel;
	if (Sections.IsEmpty())
	{
		return NameLabel;
	}

	if (SlotName != NAME_None)
	{
		NameLabel.Append(SlotName.ToString());
	}

	if (Sections.Num() == 1)
	{
		NameLabel.Append(Sections[0].ContentDirectoryToMonitor.Path);
	}
	else
	{
		using namespace UE::MetaHuman::Private;
		NameLabel.Append(MultiFolderText.ToString());
	}

	return NameLabel;
}

void SMetaHumanCharacterEditorAssetViewsPanel::PopulateSlotNames()
{
	AssetViewsSlotNames.Reset();
	if (AssetViewSections.Get().IsEmpty() || !bAllowSlots)
	{
		AssetViewsSlotNames.AddUnique(NAME_None);
	}
	else
	{
		for (const FMetaHumanCharacterAssetsSection& Section : AssetViewSections.Get())
		{
			AssetViewsSlotNames.AddUnique(Section.SlotName);
		}
	}
}

TArray<FMetaHumanCharacterAssetsSection> SMetaHumanCharacterEditorAssetViewsPanel::GetSectionsBySlotName(const FName& SlotName) const
{
	TArray<FMetaHumanCharacterAssetsSection> Sections;
	for (const FMetaHumanCharacterAssetsSection& Section : AssetViewSections.Get())
	{
		if (Section.SlotName == SlotName)
		{
			Sections.AddUnique(Section);
		}
	}

	return Sections;
}

TMap<FString, float> SMetaHumanCharacterEditorAssetViewsPanel::GetAssetViewLabelToScrollOffsetMap() const
{
	TMap<FString, float> LabelToScrollOffsetMap;
	for (const TSharedPtr<SMetaHumanCharacterEditorAssetsView>& AssetView : AssetViews)
	{
		if (AssetView.IsValid())
		{
			LabelToScrollOffsetMap.FindOrAdd(AssetView->GetLabel(), AssetView->GetScrollOffset());
		}
	}
	return LabelToScrollOffsetMap;
}

void SMetaHumanCharacterEditorAssetViewsPanel::OnSearchBoxTextChanged(const FText& InText)
{
	for (const TSharedPtr<SMetaHumanCharacterEditorAssetsView>& AssetView : AssetViews)
	{
		if (AssetView.IsValid())
		{
			AssetView->SetFilter(InText);
		}
	}
}

void SMetaHumanCharacterEditorAssetViewsPanel::OnItemSelectionChanged(TSharedPtr<FMetaHumanCharacterAssetViewItem> InSelectedItem, ESelectInfo::Type InSelectInfo)
{
	if (InSelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if (!bAllowSlotMultiSelection)
	{
		for (const TSharedPtr<SMetaHumanCharacterEditorAssetsView>& AssetView : AssetViews)
		{
			if (!AssetView.IsValid())
			{
				continue;
			}

			const TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> SelectedItems = AssetView->GetSelectedItems();
			const bool bIsSelectedItemInAssetView = SelectedItems.ContainsByPredicate(
				[InSelectedItem](const TSharedPtr<FMetaHumanCharacterAssetViewItem>& SelectedItem)
				{
					return SelectedItem->AssetData == InSelectedItem->AssetData;
				});

			if (bIsSelectedItemInAssetView || SelectedItems.IsEmpty())
			{
				continue;
			}

			AssetView->ClearSelection();
		}
	}

	OnSelectionChangedDelegate.ExecuteIfBound(InSelectedItem, InSelectInfo);
}

void SMetaHumanCharacterEditorAssetViewsPanel::OpenProjectSettings()
{
	const UMetaHumanCharacterEditorSettings* MetaHumanEditorSettings = GetDefault<UMetaHumanCharacterEditorSettings>();
	if (MetaHumanEditorSettings)
	{
		ISettingsModule& SettingsModule = FModuleManager::GetModuleChecked<ISettingsModule>("Settings");
		SettingsModule.ShowViewer(
			MetaHumanEditorSettings->GetContainerName(), 
			MetaHumanEditorSettings->GetCategoryName(), 
			MetaHumanEditorSettings->GetSectionName());
	}
}

void SMetaHumanCharacterEditorAssetViewsPanel::ToggleShowFolders()
{
	bShowFolders = !bShowFolders;

	RequestRefresh();
}

void SMetaHumanCharacterEditorAssetViewsPanel::Refresh()
{
	RefreshAssetViewsTimerHandle.Invalidate();

	// Remember the last selection before refreshing the view
	const TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> LastSelectedItems = GetSelectedItems();
	const TMap<FString, float> AssetViewLabelToScrollOffsetMap = GetAssetViewLabelToScrollOffsetMap();

	MakeAssetViewsPanel();

	// Update filters
	const FText FilterText = SearchBox->GetText();
	OnSearchBoxTextChanged(FilterText);

	// Synch scroll bars and selection
	for (const TSharedPtr<SMetaHumanCharacterEditorAssetsView>& AssetView : AssetViews)
	{
		if (!AssetView.IsValid())
		{
			continue;
		}

		const FString Label = AssetView->GetLabel();
		if (AssetViewLabelToScrollOffsetMap.Contains(Label))
		{
			const float& ScrollOffset = AssetViewLabelToScrollOffsetMap.FindChecked(Label);
			AssetView->SetScrollOffset(ScrollOffset);
		}

		const TArray<TSharedPtr<FMetaHumanCharacterAssetViewItem>> Items = AssetView->GetItems();
		for (const TSharedPtr<FMetaHumanCharacterAssetViewItem>& Item : Items)
		{
			if (!Item.IsValid())
			{
				continue;
			}

			const bool bSelectItem = LastSelectedItems.ContainsByPredicate(
				[Item](const TSharedPtr<FMetaHumanCharacterAssetViewItem>& SelectedItem)
				{
					return SelectedItem.IsValid() && SelectedItem->AssetData.ToSoftObjectPath() == Item->AssetData.ToSoftObjectPath();
				});

			if (bSelectItem)
			{
				AssetView->SetItemSelection(Item, bSelectItem, ESelectInfo::Direct);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
