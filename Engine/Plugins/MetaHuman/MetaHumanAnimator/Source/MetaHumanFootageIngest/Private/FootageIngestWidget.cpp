// Copyright Epic Games, Inc. All Rights Reserved.

#include "FootageIngestWidget.h"
#include "CaptureManagerLog.h"
#include "MetaHumanCaptureSource.h"
#include "MetaHumanTakeData.h"
#include "CaptureData.h"
#include "MetaHumanFootageRetrievalWindowStyle.h"
#include "Misc/FileHelper.h"
#include "CaptureSourcesWidget.h"
#include "MetaHumanCaptureEvents.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Views/STileView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Navigation/SBreadcrumbTrail.h"
#include "SPositiveActionButton.h"
#include "SSimpleComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

#include "Brushes/SlateImageBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateTypes.h" //added for Close button on queue items (can it be fetched through AppStyle.h?)

#ifdef TARGET_PATH_PICKER
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h" //to be able to use FPathPickerConfig
#include "IContentBrowserDataModule.h" 
#include "SlateFwd.h"
#endif//TARGET_PATH_PICKER

#include "UObject/Object.h"
#include "UObject/ConstructorHelpers.h"
#include "ImgMediaSource.h"

#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "ImageUtils.h"
#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "Internationalization/BreakIterator.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Async/IAsyncProgress.h"
#include "Misc/MessageDialog.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Utils/AppleDeviceList.h"
#include "ImageSequenceTimecodeUtils.h"

#include "FileHelpers.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ObjectTools.h"
#endif


#define LOCTEXT_NAMESPACE "FootageIngestWidget"

namespace FootageIngestDialogDefs
{
	constexpr float TakeTileHeight = 153;
	constexpr float TakeTileWidth = 102;
	constexpr float QueuedTakeThumbnailSize = 64;
	constexpr float CloseButtonPadding = 0.85f;
	constexpr float ThumbnailPadding = 5.0f;
}

/**
 * Single thumbnail tile for a take in the take tile view.
 */
class SFootageTakeTile : public STableRow<TSharedPtr<FFootageTakeItem>>
{
public:
	SLATE_BEGIN_ARGS(SFootageTakeTile)
		: _ThumbnailPadding(0)
		, _ItemWidth(16)
	{}
		SLATE_ARGUMENT(TSharedPtr<FFootageTakeItem>, Item)
		/** Current size of the thumbnail that was generated */
		SLATE_ATTRIBUTE(EThumbnailSize, CurrentThumbnailSize)
		/** How much padding to allow around the thumbnail */
		SLATE_ARGUMENT(float, ThumbnailPadding)
		/** The width of the item */
		SLATE_ATTRIBUTE(float, ItemWidth)

	SLATE_END_ARGS()

	static TSharedRef<ITableRow> BuildTile(TSharedPtr<FFootageTakeItem> InItem, const TSharedRef<STableViewBase>& InOwnerTable)
	{
		if (!ensure(InItem.IsValid()))
		{
			return SNew(STableRow<TSharedPtr<FFootageTakeItem>>, InOwnerTable);
		}

		return SNew(SFootageTakeTile, InOwnerTable)
			.Item(InItem)
			.ThumbnailPadding(FootageIngestDialogDefs::ThumbnailPadding)
			.CurrentThumbnailSize(EThumbnailSize::Medium);
	}

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
	{
		check(InArgs._Item.IsValid());
		Item = InArgs._Item;
		ThumbnailPadding = InArgs._ThumbnailPadding;
		CurrentThumbnailSize = InArgs._CurrentThumbnailSize;

		static const IConsoleVariable* CVarEnableContentBrowserNewStyle = IConsoleManager::Get().FindConsoleVariable(TEXT("ContentBrowser.EnableNewStyle"));
		const bool bEnableContentBrowserNewStyle = CVarEnableContentBrowserNewStyle && CVarEnableContentBrowserNewStyle->GetBool();

		if (bEnableContentBrowserNewStyle
			&& CurrentThumbnailSize.IsSet() && CurrentThumbnailSize.Get() == EThumbnailSize::XLarge)
		{
			CurrentThumbnailSize = EThumbnailSize::Huge;
		}

		InitializeAssetNameHeights();
		
		STableRow::Construct(
			STableRow::FArguments()
			.Style(FAppStyle::Get(), "ProjectBrowser.TableRow")
			.Padding(2.0f)
			.Content()
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.Padding(FMargin(0.0f, 0.0f, 5.0f, 5.0f))
					.IsEnabled(this, &SFootageTakeTile::IsWidgetEnabled)
					.BorderImage(FAppStyle::Get().GetBrush("ProjectBrowser.ProjectTile.DropShadow"))
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SVerticalBox)
							// Thumbnail
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(SBox)
								.WidthOverride(FootageIngestDialogDefs::TakeTileWidth)
								.HeightOverride(FootageIngestDialogDefs::TakeTileWidth) // use width on purpose, this is a square
								[
									SNew(SScaleBox)
									.Stretch(EStretch::ScaleToFit)
									[
										SNew(SBorder)
										.Padding(FMargin(0))
										.BorderImage(FAppStyle::Get().GetBrush("ProjectBrowser.ProjectTile.ThumbnailAreaBackground"))
										.HAlign(HAlign_Fill)
										.VAlign(VAlign_Fill)
										[
											SNew(SImage)
											.Image_Lambda([this]() -> const FSlateBrush* {
												if (Item->PreviewSet)
												{
													return Item->PreviewImage.Get();
												}
												return FAppStyle::Get().GetBrush("AppIcon.Small");
											})
										]
									]
								]
							]
							// Name and date
							+ SVerticalBox::Slot()
							[
								SNew(SBorder)
								.Padding(FMargin(2.0f, 3.0f))
								.BorderImage(this, &SFootageTakeTile::GetNameAreaBackgroundBrush)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.Padding(2.0f, 2.0f, 0.0f, 0.0f)
									.VAlign(VAlign_Top)
									[
										SNew(SBox)
										.MaxDesiredHeight(this, &SFootageTakeTile::GetNameAreaMaxDesiredHeight)
										[
											SNew(STextBlock)
											.Font(this, &SFootageTakeTile::GetThumbnailFont)
											.Text(InArgs._Item->Name)
											.WrapTextAt(FootageIngestDialogDefs::TakeTileWidth - 4.0f)
											.Justification(IsFolder() ? ETextJustify::Center : ETextJustify::Left)
											.LineBreakPolicy(FBreakIterator::CreateCamelCaseBreakIterator())
											.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
											.ColorAndOpacity(this, &SFootageTakeTile::GetNameAreaTextColor)
										]
									]
									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
									[
										SNew(STextBlock)
										.Font(GetDateFontStyle())
										.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
										.WrapTextAt(FootageIngestDialogDefs::TakeTileWidth - 4.0f)
										.LineBreakPolicy(FBreakIterator::CreateLineBreakIterator())
										.Text(GetDateTimeText(InArgs._Item->CaptureSource, InArgs._Item->TakeId))
										.ColorAndOpacity(this, &SFootageTakeTile::GetDateAreaTextColor)
									]
								]
							]
						]
						+ SOverlay::Slot()
						[
							SNew(SImage)
							.Visibility(EVisibility::HitTestInvisible)
							.Image(this, &SFootageTakeTile::GetSelectionOutlineBrush)
						]
						+ SOverlay::Slot()
						[
							SNew(SImage)
							.Visibility(this, &SFootageTakeTile::ShowWarningBox)
							.Image(FAppStyle::Get().GetBrush("RoundedWarning"))
						]
					]
				]
				+ SOverlay::Slot()
				.VAlign(EVerticalAlignment::VAlign_Top)
				.HAlign(EHorizontalAlignment::HAlign_Left)
				.Padding(4)
				[
					SNew(SImage)
					.Visibility(this, &SFootageTakeTile::ShowWarningBox)
					.Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor"))
				]
			],
			InOwnerTable
		);
	}

private:

	bool IsFolder() const
	{
		return false; //none of the TakeView items are folders, but keeping it as a method as we might want to allow this in the future
	}

	const FSlateBrush* GetSelectionOutlineBrush() const
	{
		const bool bIsSelected = IsSelected();
		const bool bIsTileHovered = IsHovered();

		if (bIsSelected && bIsTileHovered)
		{
			static const FName SelectedHover("ProjectBrowser.ProjectTile.SelectedHoverBorder");
			return FAppStyle::Get().GetBrush(SelectedHover);
		}
		else if (bIsSelected)
		{
			static const FName Selected("ProjectBrowser.ProjectTile.SelectedBorder");
			return FAppStyle::Get().GetBrush(Selected);
		}
		else if (bIsTileHovered)
		{
			static const FName Hovered("ProjectBrowser.ProjectTile.HoverBorder");
			return FAppStyle::Get().GetBrush(Hovered);
		}

		return FStyleDefaults::GetNoBrush();
	}

	const FSlateBrush* GetNameAreaBackgroundBrush() const
	{
		const bool bIsSelected = IsSelected();
		const bool bIsRowHovered = IsHovered();

		if (bIsSelected && bIsRowHovered)
		{
			static const FName SelectedHover("ProjectBrowser.ProjectTile.NameAreaSelectedHoverBackground");
			return FAppStyle::Get().GetBrush(SelectedHover);
		}
		else if (bIsSelected)
		{
			static const FName Selected("ProjectBrowser.ProjectTile.NameAreaSelectedBackground");
			return FAppStyle::Get().GetBrush(Selected);
		}
		else if (bIsRowHovered)
		{
			static const FName Hovered("ProjectBrowser.ProjectTile.NameAreaHoverBackground");
			return FAppStyle::Get().GetBrush(Hovered);
		}

		return FAppStyle::Get().GetBrush("ProjectBrowser.ProjectTile.NameAreaBackground");
	}

	FSlateColor GetNameAreaTextColor() const
	{
		const bool bIsSelected = IsSelected();
		const bool bIsRowHovered = IsHovered();

		if (bIsSelected || bIsRowHovered)
		{
			return FStyleColors::White;
		}

		return FSlateColor::UseForeground();
	}

	FSlateColor GetDateAreaTextColor() const
	{
		const bool bIsSelected = IsSelected();
		const bool bIsRowHovered = IsHovered();

		if (bIsSelected || bIsRowHovered)
		{
			return FStyleColors::White;
		}

		return FSlateColor::UseSubduedForeground();
	}

	FSlateFontInfo GetDateFontStyle() const
	{
		FSlateFontInfo FontInfo = FAppStyle::Get().GetFontStyle("ContentBrowser.ClassFont");
		FontInfo.Size = 6;
		return FontInfo;
	}

	FText GetDateTimeText(TSharedPtr<FFootageCaptureSource> InCaptureSource, TakeId InTakeId) const
	{
		FMetaHumanTakeInfo TakeInfo;
		InCaptureSource->GetIngester().GetTakeInfo(InTakeId, TakeInfo);
		return FText::AsDateTime(TakeInfo.Date, EDateTimeStyle::Short, EDateTimeStyle::Default);
	}

	TSharedPtr<IToolTip> PrepareTooltipWidget() const 
	{
		TSharedPtr<SToolTip> ToolTip = 
			SNew(SToolTip)
			.TextMargin(1.0f)
			.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ToolTipBorder"));

		ToolTip->SetContentWidget(CreateToolTipContent());

		return ToolTip;
	}

	TSharedRef<SWidget> CreateToolTipContent() const
	{
		TSharedRef<SVerticalBox> InfoBox = SNew(SVerticalBox);

		FillTakeInfoInTooltip(InfoBox, Item->CaptureSource, Item->TakeId);

		TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox);
		// Name
		ContentBox->AddSlot()
			.AutoHeight()
			.Padding(0, 0, 0, 2)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(SBorder)
				.Padding(4)
				.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::Format(NSLOCTEXT("FootageTakeTile", "FootageTakeTileTitle", "{0}"), Item->Name))
						.Font(FAppStyle::GetFontStyle("ContentBrowser.TileViewTooltip.NameFont"))
					]
				]
			];

		// Content
		ContentBox->AddSlot()
			.AutoHeight()
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(SBorder)
				.Padding(4)
				.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
				[
					InfoBox
				]
			];

		if (Item->Status == EFootageTakeItemStatus::Warning)
		{
			// Warning
			ContentBox->AddSlot()
				.AutoHeight()
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(SBorder)
					.Padding(4)
					.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2, 0, 0, 0)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4, 0, 0, 0)
						[
							SNew(STextBlock)
							.Text(this, &SFootageTakeTile::GetWarningText)
						]
					]
				];
		}

		return SNew(SBorder)
			.Padding(6)
			.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.NonContentBorder"))
			[
				ContentBox
			];
	}

	void AddToInfoBox(const TSharedRef<SVerticalBox>& InInfoBox, const FText& InKey, const FText& InValue) const
	{
		InInfoBox->AddSlot()
			.AutoHeight()
			.Padding(1)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 4, 0)
				[
					SNew(STextBlock)
					.Text(FText::Format(NSLOCTEXT("FootageTakeTile", "FootageTakeTileFormat", "{0}:"), InKey))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(InValue)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.WrapTextAt(700.0f)
				]
			];
	}

	template<typename Function>
	void AddToInfoBox(const TSharedRef<SVerticalBox>& InInfoBox, const FText& InKey, Function InLambda) const
	{
		InInfoBox->AddSlot()
			.AutoHeight()
			.Padding(1)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 4, 0)
				[
					SNew(STextBlock)
					.Text(FText::Format(NSLOCTEXT("FootageTakeTile", "FootageTakeTileFormat", "{0}:"), InKey))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda(InLambda)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.WrapTextAt(700.0f)
				]
			];
	}

	void FillTakeInfoInTooltip(TSharedRef<SVerticalBox> InInfoBox, TSharedPtr<FFootageCaptureSource> InCaptureSource, TakeId InTakeId) const
	{
		FMetaHumanTakeInfo TakeInfo;
		InCaptureSource->GetIngester().GetTakeInfo(InTakeId, TakeInfo);

		FText DateTime = FText::AsDateTime(TakeInfo.Date, EDateTimeStyle::Short, EDateTimeStyle::Default);
		AddToInfoBox(InInfoBox, NSLOCTEXT("FootageTakeTile", "FootageTakeTileDate", "Date"), DateTime);

		AddToInfoBox(InInfoBox, NSLOCTEXT("FootageTakeTile", "FootageTakeTileNumberFrames", "Number of Frames"), FText::AsNumber(TakeInfo.NumFrames));
		AddToInfoBox(InInfoBox, NSLOCTEXT("FootageTakeTile", "FootageTakeTileFrameRate", "Frame Rate"), FText::AsNumber(TakeInfo.FrameRate));
		AddToInfoBox(InInfoBox, NSLOCTEXT("FootageTakeTile", "FootageTakeTileResolution", "Resolution"), FText::FromString(TakeInfo.Resolution.ToString()));

		// If a user friendly display name is available in the AppleDeviceList use that for 'Device Model', otherwise use the raw device model string.
		const FString& DeviceModel = TakeInfo.DeviceModel;
		if (!DeviceModel.IsEmpty())
		{
			FString DisplayName = DeviceModel;
			if (FAppleDeviceList::DeviceMap.Contains(DeviceModel))
			{
				DisplayName = FAppleDeviceList::DeviceMap[DeviceModel];
			}
			AddToInfoBox(InInfoBox, NSLOCTEXT("FootageTakeTile", "FootageTakeTileDeviceModel", "Device Model"), FText::FromString(DisplayName));
		}
	}

	void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		SetToolTip(PrepareTooltipWidget());

		STableRow<TSharedPtr<FFootageTakeItem>>::OnMouseEnter(MyGeometry, MouseEvent);
	}

	void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		SetToolTip(nullptr);
		FSlateApplication::Get().CloseToolTip();

		STableRow<TSharedPtr<FFootageTakeItem>>::OnMouseLeave(MouseEvent);
	}

	void OnFocusLost(const FFocusEvent& InFocusEvent) override
	{
		SetToolTip(nullptr);
		FSlateApplication::Get().CloseToolTip();

		STableRow<TSharedPtr<FFootageTakeItem>>::OnFocusLost(InFocusEvent);
	}

	FSlateFontInfo GetThumbnailFont() const
	{
		/* for future:
		   the following code will be useful if we ever allow thumbnails of different sizes,
		   for now it is causing problems by shrinking fonts when it is not desired

		FOptionalSize ThumbSize = GetThumbnailBoxSize();
		if (ThumbSize.IsSet())
		{
			float Size = ThumbSize.Get();
			if (Size < 50)
			{
				const static FName SmallFontName("ContentBrowser.AssetTileViewNameFontVerySmall");
				return FAppStyle::GetFontStyle(SmallFontName);
			}
			else if (Size < 85)
			{
				const static FName SmallFontName("ContentBrowser.AssetTileViewNameFontSmall");
				return FAppStyle::GetFontStyle(SmallFontName);
			}
		}*/

		const static FName RegularFont("ContentBrowser.AssetTileViewNameFont");
		return FAppStyle::GetFontStyle(RegularFont);
	}

	FOptionalSize GetNameAreaMaxDesiredHeight() const
	{
		FOptionalSize Size = AssetNameHeights[(int32)CurrentThumbnailSize.Get()];
		return Size;
	}

	void InitializeAssetNameHeights()
	{
		// The height of the asset name field for each thumbnail size
		static bool bInitializedHeights = false;

		if (!bInitializedHeights)
		{
			AssetNameHeights[(int32)EThumbnailSize::Tiny] = 0;

			{
				const static FName SmallFontName("ContentBrowser.AssetTileViewNameFontSmall");
				FSlateFontInfo Font = FAppStyle::GetFontStyle(SmallFontName);
				TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				SmallFontHeight = FontMeasureService->GetMaxCharacterHeight(Font);

				constexpr float SmallSizeMultiplier = 2;
				AssetNameHeights[(int32)EThumbnailSize::Small] = SmallFontHeight * SmallSizeMultiplier;
			}


			{
				const static FName SmallFontName("ContentBrowser.AssetTileViewNameFont");
				FSlateFontInfo Font = FAppStyle::GetFontStyle(SmallFontName);
				TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				RegularFontHeight = FontMeasureService->GetMaxCharacterHeight(Font);

				constexpr float MediumSizeMultiplier = 3;
				constexpr float LargeSizeMultiplier = 4;
				constexpr float XLargeSizeMultiplier = 5;
				constexpr float HugeSizeMultiplier = 6;

				AssetNameHeights[(int32)EThumbnailSize::Medium] = RegularFontHeight * MediumSizeMultiplier;
				AssetNameHeights[(int32)EThumbnailSize::Large] = RegularFontHeight * LargeSizeMultiplier;

				static const IConsoleVariable* CVarEnableContentBrowserNewStyle = IConsoleManager::Get().FindConsoleVariable(TEXT("ContentBrowser.EnableNewStyle"));
				const bool bEnableContentBrowserNewStyle = CVarEnableContentBrowserNewStyle && CVarEnableContentBrowserNewStyle->GetBool();

				if (bEnableContentBrowserNewStyle)
				{
					AssetNameHeights[(int32)EThumbnailSize::XLarge] = RegularFontHeight * XLargeSizeMultiplier;
					AssetNameHeights[(int32)EThumbnailSize::Huge] = RegularFontHeight * HugeSizeMultiplier;
				}
				else
				{
					AssetNameHeights[(int32)EThumbnailSize::Huge] = RegularFontHeight * XLargeSizeMultiplier;
				}
			}

			bInitializedHeights = true;
		}
	}

	FOptionalSize GetThumbnailBoxSize() const
	{
		return FOptionalSize(ItemWidth.Get() - ThumbnailPadding);
	}

	bool IsWidgetEnabled() const
	{
		return !CheckForWarnings();
	}

	bool CheckForWarnings() const
	{
		return Item->Status == EFootageTakeItemStatus::Warning;
	}

	EVisibility ShowWarningBox() const
	{
		return CheckForWarnings() ? EVisibility::Visible : EVisibility::Hidden;
	}

	FText GetWarningText() const
	{
		return FText::FromString(Item->StatusMessage);
	}

private:

	TSharedPtr<FFootageTakeItem> Item;

	/** The width of the item. Used to enforce a square thumbnail. */
	TAttribute<float> ItemWidth;
	
	/** Max name height for each thumbnail size */
	static float AssetNameHeights[(int32)EThumbnailSize::MAX];

	/** Regular thumbnail font size */
	static float RegularFontHeight;

	/** Small thumbnail font size */
	static float SmallFontHeight;

	/** The padding for the thumbnail */
	float ThumbnailPadding;

	/** Current thumbnail size when this widget was generated */
	TAttribute<EThumbnailSize> CurrentThumbnailSize;
};

float SFootageTakeTile::AssetNameHeights[(int32)EThumbnailSize::MAX];
float SFootageTakeTile::RegularFontHeight(0);
float SFootageTakeTile::SmallFontHeight(0);

class SFootageQueuedTakeRow : public STableRow<TSharedPtr<FFootageTakeItem>>
{
public:
	SLATE_BEGIN_ARGS(SFootageQueuedTakeRow) {}
		SLATE_ARGUMENT(TSharedPtr<FFootageTakeItem>, Item)

		SLATE_ARGUMENT(TWeakPtr<SFootageIngestWidget>, FootageIngestWidget)
	SLATE_END_ARGS()

	static TSharedRef<ITableRow> BuildRow(TSharedPtr<FFootageTakeItem> InItem, const TSharedRef<STableViewBase>& InOwnerTable, TWeakPtr<SFootageIngestWidget> InOwner)
	{
		if (!ensure(InItem.IsValid()))
		{
			return SNew(STableRow<TSharedPtr<FFootageTakeItem>>, InOwnerTable);
		}

		return SNew(SFootageQueuedTakeRow, InOwnerTable)
			.Item(InItem)
			.FootageIngestWidget(InOwner);
	}

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
	{
		check(InArgs._Item.IsValid());

		const FDockTabStyle* DockTabStyle = &FAppStyle::Get().GetWidgetStyle<FDockTabStyle>("Docking.Tab");
		const FButtonStyle* const CloseButtonStyle = &GetDockTabStyle(DockTabStyle).CloseButtonStyle;

		STableRow::Construct(
			STableRow::FArguments()
			.Padding(2)
			.Content()
			[
				SNew(SBorder)
				.Padding(FMargin(2))
				.BorderImage(FAppStyle::GetBrush("Brushes.Header"))
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SVerticalBox)
								// Thumbnail
								+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.Padding(4)
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										SNew(SBorder)
										.Padding(FMargin(0))
										.BorderImage(GetBlackBox())
										.ColorAndOpacity(FLinearColor::Black)
										.HAlign(HAlign_Fill)
										.VAlign(VAlign_Fill)
									]
									+SOverlay::Slot()
									[
										SNew(SBox)
										.WidthOverride(FootageIngestDialogDefs::QueuedTakeThumbnailSize)
										.HeightOverride(FootageIngestDialogDefs::QueuedTakeThumbnailSize)
										[
											SNew(SScaleBox)
											.Stretch(EStretch::ScaleToFit)
											[
												SNew(SBorder)
												.Padding(FMargin(0))
												.BorderImage(FAppStyle::Get().GetBrush("ProjectBrowser.ProjectTile.ThumbnailAreaBackground"))
												.HAlign(HAlign_Fill)
												.VAlign(VAlign_Fill)
												[
													SNew(SImage)
													.Image_Lambda([Item = InArgs._Item]() -> const FSlateBrush* {
														if (Item->PreviewSet)
														{
															return Item->PreviewImage.Get();
														}
														return FAppStyle::Get().GetBrush("AppIcon.Small");
													})
												]
											]
										]
									]
								]
							]

							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.FillWidth(1.f)
							.Padding(2)
							[
								SNew(SVerticalBox)

								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Margin(2)
									.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
									.Text(InArgs._Item->CaptureSource->Name)
									]

								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Margin(2)
									.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
									.Text(InArgs._Item->Name)
								]

								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(2.0f)
									[
										SNew(SImage)
										.Image(this, &SFootageQueuedTakeRow::GetStatusIcon, InArgs._Item)
										.ToolTipText(this, &SFootageQueuedTakeRow::GetStatusTooltipText, InArgs._Item)
									]
									+ SHorizontalBox::Slot()
									[
										SNew(SBox)
										.Padding(2)
										[
											SNew(SOverlay)
											// Downloading/ingesting caption
											+ SOverlay::Slot()
											.VAlign(VAlign_Center)
											.HAlign(HAlign_Left)
											[
												SNew(STextBlock)
												.Justification(ETextJustify::Left)
												.ColorAndOpacity(FSlateColor::UseForeground())
												.Text(this, &SFootageQueuedTakeRow::GetProgressBarText, InArgs._Item)
												.ToolTipText(this, &SFootageQueuedTakeRow::GetStatusTooltipText, InArgs._Item)
												.Visibility_Lambda([Item = InArgs._Item]()
												{
													return Item->Status == EFootageTakeItemStatus::Ingest_Active || Item->Status == EFootageTakeItemStatus::Queued ? EVisibility::Hidden : EVisibility::Visible;
												})
											]
											+ SOverlay::Slot()
											.VAlign(VAlign_Center)
											.HAlign(HAlign_Fill)
											[
												SNew(SProgressBar)
												.Percent_Lambda([Item = InArgs._Item]()
												{
													if (Item->CaptureSource)
													{
														if (Item->Status == EFootageTakeItemStatus::Ingest_Active ||
															Item->Status == EFootageTakeItemStatus::Ingest_Failed)
														{
															return Item->CaptureSource->GetIngester().GetProcessingProgress(Item->TakeId).GetValue();
														}
														else if (Item->Status == EFootageTakeItemStatus::Ingest_Succeeded || Item->Status == EFootageTakeItemStatus::Ingest_Succeeded_with_Warnings)
														{
															return 1.0f;
														}
													}
													return 0.f;
												})
												.Visibility_Lambda([Item = InArgs._Item]()
												{
													return Item->Status == EFootageTakeItemStatus::Ingest_Active || Item->Status == EFootageTakeItemStatus::Queued ? EVisibility::Visible : EVisibility::Hidden;
												})
											]
											// Downloading/ingesting caption
											+ SOverlay::Slot()
											.VAlign(VAlign_Center)
											.HAlign(HAlign_Fill)
											[
												SNew(STextBlock)
												.Margin(FMargin(0, 0))
												.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
												.TextStyle(FAppStyle::Get(), "ButtonText")
												.Justification(ETextJustify::Center)
												.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
												.ColorAndOpacity(FSlateColor(FLinearColor::White))
												.Text(this, &SFootageQueuedTakeRow::GetProgressBarText, InArgs._Item)
												.Visibility_Lambda([Item = InArgs._Item]()
												{
													return Item->Status == EFootageTakeItemStatus::Ingest_Active || Item->Status == EFootageTakeItemStatus::Queued ? EVisibility::Visible : EVisibility::Hidden;
												})
											]
										]
									]
								]
							]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBox)
								.Padding(4,2)
								[
									SNew(SImage)
									.Image(FAppStyle::Get().GetBrush("Icons.FolderClosed"))
									.ColorAndOpacity(FSlateColor::UseForeground())
									.ToolTipText(InArgs._Item->DestinationFolder) //in case the path is clipped off
								]
							]

							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							[
								SNew(STextBlock)
								.Margin(2)
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								.Text(InArgs._Item->DestinationFolder)
								.ToolTipText(InArgs._Item->DestinationFolder) //in case the path is clipped off
							]
						]
					]
					//Close button
					+ SOverlay::Slot()
					.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SFootageQueuedTakeRow::GetCloseButtonPadding)))
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Fill)
					[
						SNew(SHorizontalBox)
						.Visibility(EVisibility::Visible)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(0.0f, 1.0f)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
#ifdef CANCEL_BUTTON_FOR_INDIVIDUAL_TASKS
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(CloseButtonStyle)
							.OnClicked(this, &SFootageQueuedTakeRow::OnCloseButtonClicked, InArgs._Item, InArgs._FootageIngestWidget)
							.ContentPadding(FMargin(0.0, 1.5, 0.0, 0.0))
							.ToolTipText(this, &SFootageQueuedTakeRow::GetCloseButtonToolTipText, InArgs._Item)
							.Visibility(this, &SFootageQueuedTakeRow::HandleIsCloseButtonVisible)
							[
								SNew(SSpacer)
								.Size(CloseButtonStyle->Normal.ImageSize)
							]
						]
#endif //CANCEL_BUTTON_FOR_INDIVIDUAL_TASKS
					]
				]
			],
			InOwnerTable
			);
	}

private:

	static FSlateBoxBrush BlackBox;

	FReply OnCloseButtonClicked(TSharedPtr<FFootageTakeItem> InItem, TWeakPtr<SFootageIngestWidget> InFootageIngestWidget)
	{
		if (InItem->Status == EFootageTakeItemStatus::Ingest_Active)
		{
			TSharedPtr<FFootageCaptureSource> FootageCaptureSource = InItem->CaptureSource;
			TArray<int32> ItemsToClose;
			ItemsToClose.Add(InItem->TakeId);
			FootageCaptureSource->GetIngester().CancelProcessing(ItemsToClose);
			InItem->Status = EFootageTakeItemStatus::Ingest_Canceled; //signal to Tick() method to remove it from the queue
		}
		else
		{
			if (TSharedPtr<SFootageIngestWidget> FootageIngestLocal = InFootageIngestWidget.Pin(); FootageIngestLocal)
			{
				FootageIngestLocal->UnqueueTake(InItem, true);
			}
		}
		
		return FReply::Handled();
	}

	FText GetCloseButtonToolTipText(TSharedPtr<FFootageTakeItem> InItem) const
	{
		if (InItem->Status == EFootageTakeItemStatus::Queued || InItem->Status == EFootageTakeItemStatus::Ingest_Active)
		{
			return FText(NSLOCTEXT("FootageIngestQueueItem", "CloseButtonActiveOrQueuedItemToolTip", "Cancel import of this item"));
		}
		else
		{
			return FText(NSLOCTEXT("FootageIngestQueueItem", "CloseButtonFinishedItemToolTip", "Remove this item from queue"));
		}
	}

	const FSlateBrush* GetStatusIcon(TSharedPtr<FFootageTakeItem> InItem) const
	{
		switch (InItem->Status)
		{
		case EFootageTakeItemStatus::Queued:
			return FAppStyle::Get().GetBrush("Icons.InfoWithColor");
		case EFootageTakeItemStatus::Ingest_Active:
			return FAppStyle::Get().GetBrush("Icons.InfoWithColor");
		case EFootageTakeItemStatus::Ingest_Failed:
			return FAppStyle::Get().GetBrush("Icons.ErrorWithColor");
		case EFootageTakeItemStatus::Ingest_Succeeded:
			return FAppStyle::Get().GetBrush("Icons.SuccessWithColor");
		case EFootageTakeItemStatus::Ingest_Canceled:
			return FAppStyle::Get().GetBrush("Icons.WarningWithColor");
		case EFootageTakeItemStatus::Ingest_Succeeded_with_Warnings:
			//NOTE: added in case it is required, if not, it can be removed
			return FAppStyle::Get().GetBrush("Icons.WarningWithColor");
		}
		return nullptr;
	}

	FText GetStatusTooltipText(TSharedPtr<FFootageTakeItem> InItem) const
	{
		switch (InItem->Status)
		{
		case EFootageTakeItemStatus::Queued:
			return LOCTEXT("IngestStatusIconTooltipQueued", "The take is ready to import");
		case EFootageTakeItemStatus::Ingest_Active:
			return LOCTEXT("IngestStatusIconTooltipActive", "The take is currently being imported");
		case EFootageTakeItemStatus::Ingest_Failed:
			if (InItem->StatusMessage.IsEmpty())
			{
				return LOCTEXT("IngestStatusIconTooltipFailed", "The take failed to be imported");
			}

			return FText::FromString(InItem->StatusMessage);
		case EFootageTakeItemStatus::Ingest_Succeeded:
			return LOCTEXT("IngestStatusIconTooltipSucceeded", "The take has been imported");
		case EFootageTakeItemStatus::Ingest_Canceled:
			return LOCTEXT("IngestStatusIconTooltipCancelled", "The take has been canceled");
		case EFootageTakeItemStatus::Ingest_Succeeded_with_Warnings:
			if (InItem->StatusMessage.IsEmpty())
			{
				return LOCTEXT("IngestStatusIconTooltipSucceededWithWarnings", "The take has been imported with warnings");
			}

			return FText::FromString(InItem->StatusMessage);
		}
		return FText();
	}

	const FDockTabStyle& GetDockTabStyle( const FDockTabStyle* DockTabStyle ) const
	{
		return *DockTabStyle;
	}

	EVisibility HandleIsCloseButtonVisible() const
	{
		return EVisibility::Visible;
	}

	FMargin GetCloseButtonPadding() const
	{
		return FootageIngestDialogDefs::CloseButtonPadding;
	}

	FText GetProgressBarText(TSharedPtr<FFootageTakeItem> InItem) const
	{
		if (InItem && InItem->CaptureSource)
		{
			if (InItem->Status == EFootageTakeItemStatus::Queued)
			{
				return LOCTEXT("FootageIngestProgressBarIngestQueued", "Queued");
			}
			if (InItem->Status == EFootageTakeItemStatus::Ingest_Active)
			{
				return InItem->CaptureSource->GetIngester().GetProcessName(InItem->TakeId);
			}
			else if (InItem->Status == EFootageTakeItemStatus::Ingest_Failed)
			{
				return LOCTEXT("FootageIngestProgressBarIngestFailed", "Failed");
			}
			else if (InItem->Status == EFootageTakeItemStatus::Ingest_Succeeded)
			{
				return LOCTEXT("FootageIngestProgressBarIngestSucceeded", "Succeeded");
			}
			else if (InItem->Status == EFootageTakeItemStatus::Ingest_Canceled)
			{
				return LOCTEXT("FootageIngestProgressBarIngestCanceled", "Canceled");
			}
			else if (InItem->Status == EFootageTakeItemStatus::Ingest_Succeeded_with_Warnings)
			{
				return LOCTEXT("FootageIngestProgressBarIngestSucceededWithWarnings", "Succeeded (with warnings)");
			}
		}
		return FText();
	}

	const FSlateBoxBrush* GetBlackBox() { return &BlackBox; };
};

FSlateBoxBrush SFootageQueuedTakeRow::BlackBox = FSlateBoxBrush(NAME_None, 0.0f, FStyleColors::Black);

void SFootageIngestWidget::Construct(const FArguments& InArgs)
{
	OwnerTab = InArgs._OwnerTab;
	OnTargetFolderAssetPathChangedDelegate = InArgs._OnTargetFolderAssetPathChanged;

	TakeViewListSource = &TakeItems_Null;
	bImportingTakes = false;

	//initially, the target path is empty because there is no capture source selected
	//the text box (breadcrumbs trail in future) is filled in in OnTargetPathChange
	TargetFolderPickerAssetPath = FText::FromString("");
	TargetFolderPickerFullPathOnDisk = FText::FromString("");

#ifdef SHOW_CAPTURE_SOURCE_TOOLBAR

	FSlimHorizontalToolBarBuilder ToolbarBuilder(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), "AssetEditorToolbar");

	//for future: add CaptureSource-specific toolbar here (e.g. CaptureSource type, IP address etc...)
	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	HorizontalBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(5.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(SBox)
		];

	ToolbarBuilder.BeginSection("General");
	{
		ToolbarBuilder.AddWidget(HorizontalBox);
	}
	ToolbarBuilder.EndSection();
#endif //SHOW_CAPTURE_SOURCE_TOOLBAR

	ChildSlot
	[
		SNew(SVerticalBox)

#ifdef INGEST_UNIMPLEMENTED_UI

		// Top bar with source and path selectors
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0)
		[
			SNew(SHorizontalBox)

			// History Back Button
			+ SHorizontalBox::Slot()
			.Padding(10, 0, 0, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(1, 0))
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FootageIngestSourceHistoryBack")))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.CircleArrowLeft"))
					.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

			// History Forward Button
			+ SHorizontalBox::Slot()
				.Padding(2, 0, 0, 0)
				.AutoWidth()
				[
					SNew(SButton)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ContentPadding(FMargin(1, 0))
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FootageIngestSourceHistoryForward")))
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.CircleArrowRight"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

			// Path picker
			+ SHorizontalBox::Slot()
			.Padding(2, 0, 0, 0)
			.AutoWidth()
			.VAlign(VAlign_Fill)
			[
				SNew(SComboButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(LOCTEXT("PathPickerTooltip", "Choose a path"))
				.HasDownArrow(false)
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FootageIngestSourcePathPicker")))
				.ContentPadding(FMargin(1, 0))
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.FolderClosed"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// Path
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.FillWidth(1.0f)
			.Padding(2, 0, 0, 0)
			[
				SNew(SBreadcrumbTrail<FString>)
				.ButtonContentPadding(FMargin(2, 2))
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.DelimiterImage(FAppStyle::Get().GetBrush("Icons.ChevronRight"))
				.TextStyle(FAppStyle::Get(), "NormalText")
				.ShowLeadingDelimiter(false)
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FootageIngestSourcePath")))
			]

			// View settings
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
				.HasDownArrow(false)
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0, 0.0f)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(4.0, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Settings", "Settings"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
			.Thickness(2.0f)
		]
#endif

#ifdef SHOW_CAPTURE_SOURCE_TOOLBAR
		// Main pane
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ToolbarBuilder.MakeWidget()
		]
#endif
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0.0f)
		[
			SNew(SSplitter) //horizontal splitter separating central part with take view and ingest queue
			.PhysicalSplitterHandleSize(2.0f)

			// Center pane with search, take view, and target browser
			+ SSplitter::Slot() //horizontal splitter
			.SizeRule(SSplitter::ESizeRule::FractionOfParent)
			.Value(0.7f)
			[
				SNew(SBorder)
				.Padding(FMargin(4, 4, 0, 0))
				.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
				[
					SNew(SVerticalBox)
	
					// Top bar with take search and filters
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(EHorizontalAlignment::HAlign_Fill)
						.FillWidth(1.0)
						[
							SNew(SBox)
							[
								SAssignNew(TakeSearchBar, SSearchBox)
								.HintText(LOCTEXT("TakeSearch", "Search..."))
								.ToolTipText(LOCTEXT("TakeSearchHint", "Type here to search"))
								.OnTextChanged(this, &SFootageIngestWidget::OnTakeFilterTextChanged)
								.OnTextCommitted(this, &SFootageIngestWidget::OnTakeFilterTextCommitted)
							]
						]
#ifdef SHOW_FILTERS_FOR_SOURCE_PATH
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.VAlign(EVerticalAlignment::VAlign_Center)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.ContentPadding(FMargin(1, 0))
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FootageIngestSourceHistoryForward")))
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Filter"))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
#endif //SHOW_FILTERS_FOR_SOURCE_PATH
					]
					// Take view
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(FMargin(2, 0))
					[
						SAssignNew(TakeTileView, STileView<TSharedPtr<FFootageTakeItem>>)
						.ListItemsSource(&TakeItems_Null)
						.SelectionMode(ESelectionMode::Multi)
						.ClearSelectionOnClick(true)
						.OnSelectionChanged(this, &SFootageIngestWidget::OnTakeViewSelectionChanged)
						.ItemAlignment(EListItemAlignment::LeftAligned)
						.OnGenerateTile_Static(&SFootageTakeTile::BuildTile)
						.ItemHeight(FootageIngestDialogDefs::TakeTileHeight + 9)
						.ItemWidth(FootageIngestDialogDefs::TakeTileWidth + 9)
					]

					// Lower bar with target selection
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0)
					[
						SNew(SHorizontalBox)
#ifdef TARGET_PATH_PICKER
						// Path picker
						+ SHorizontalBox::Slot()
						.Padding(2, 2, 0, 2)
						.AutoWidth()
						.VAlign(VAlign_Fill)
						[
							SAssignNew(PathPickerButton, SSimpleComboButton)
							.Visibility(EVisibility::Visible)
							.ToolTipText(LOCTEXT("PathPickerTooltip", "Choose a path"))
							.OnGetMenuContent(this, &SFootageIngestWidget::GetPathPickerContent)
							.IsEnabled(CurrentCaptureSource != nullptr)
							.HasDownArrow(false)
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FootageIngestTargetPathPicker")))
							.Icon(FAppStyle::Get().GetBrush("Icons.FolderClosed"))
						]
#endif //TARGET_PATH_PICKER
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2, 6, 0, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TargetFolder", "Target Folder:"))
						]
#ifdef TARGET_PATH_PICKER
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.Padding(2, 2, 0, 2)
						[
							SAssignNew(TargetFolderTextBox, SEditableTextBox)
							.Text(TargetFolderPickerAssetPath)
							.ToolTipText(this, &SFootageIngestWidget::GetTargetFolderPickerPathTooltip)
							.IsEnabled(false)
						]
#else
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SAssignNew(TargetFolderTextBox, SEditableTextBox)
							.Text(LOCTEXT("TargetPlaceholder", "(CaptureSource Folder)"))
							.IsEnabled(false)
						]
#endif //TARGET_PATH_PICKER
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SAssignNew(AddToQueueButton, SPositiveActionButton)
							.Text(this, &SFootageIngestWidget::GetQueueButtonText)
							.Icon(this, &SFootageIngestWidget::GetQueueButtonIcon)
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FootageIngestAddToQueue")))
							.ToolTipText(this, &SFootageIngestWidget::GetQueueButtonTooltip)
							.OnClicked(this, &SFootageIngestWidget::OnQueueButtonClicked)
							.IsEnabled(this, &SFootageIngestWidget::IsQueueButtonEnabled)
						]
					]

					// Status bar
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0)
					[
						SNew(SBorder)
						.Padding(FMargin(2))
						.BorderImage(FAppStyle::GetBrush("Brushes.Header"))
						[
							SAssignNew(TakeStatusBarText, STextBlock)
							.Text(this, &SFootageIngestWidget::GetTakeCountText)
						]
					]
				]
			]

			// Right pane with queue
			+ SSplitter::Slot()
			.SizeRule(SSplitter::ESizeRule::FractionOfParent)
			.Value(0.3f)
			[
				SNew(SVerticalBox)

				// Button bar with start/stop controls
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					[
						SNew(SButton)
						.OnClicked(this, &SFootageIngestWidget::OnImportTakesClicked)
						.IsEnabled(this, &SFootageIngestWidget::IsImportTakesEnabled)
						.ToolTipText(LOCTEXT("FootageIngestImportAllButtonTooltip", "Import all the takes from the queue"))
						.HAlign(HAlign_Fill)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Import"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(4.0f, 0.0f)
							[
								SNew(STextBlock)
								.Clipping(EWidgetClipping::ClipToBoundsAlways)
								.Text(LOCTEXT("FootageIngestImportAllButtonLabel", "Import All"))
								.Justification(ETextJustify::Left)
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							]
						]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					[
						//The button for canceling all the takes currently being imported
						//The cancelled takes should automatically be deleted from the import list 
						SNew(SButton)
						.OnClicked(this, &SFootageIngestWidget::OnCancelAllImportClicked)
						.IsEnabled(this, &SFootageIngestWidget::IsCancelAllImportEnabled)
						.ToolTipText(LOCTEXT("FootageIngestCancelAllButtonTooltip", "Cancel all the takes that are currently importing\nand remove them from the queue"))
						.HAlign(HAlign_Fill)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("GenericStop"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(4.0f, 0.0f)
							[
								SNew(STextBlock)
								.Clipping(EWidgetClipping::ClipToBoundsAlways)
								.Text(LOCTEXT("FootageIngestStopAllButtonLabel", "Stop All"))
								.Justification(ETextJustify::Left)
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							]
						]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					[
						//The button for clearing the already imported takes (that now stay in the list)
						SNew(SButton)
						.OnClicked(this, &SFootageIngestWidget::OnClearAllImportClicked)
						.IsEnabled(this, &SFootageIngestWidget::IsClearAllImportEnabled)
						.ToolTipText(LOCTEXT("FootageIngestClearAllButtonTooltip","Clear from the queue all the takes that are not currently in progress"))
						.HAlign(HAlign_Fill)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.XCircle"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(4.0f, 0.0f)
							[
								SNew(STextBlock)
								.Clipping(EWidgetClipping::ClipToBoundsAlways)
								.Text(LOCTEXT("FootageIngestClearAllButtonLabel", "Clear All"))
								.Justification(ETextJustify::Left)
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							]
						]
					]
				]

				// Queue view
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SAssignNew(QueueListView, SListView<TSharedPtr<FFootageTakeItem>>)
					.ListItemsSource(&QueuedTakes)
					.SelectionMode(ESelectionMode::Multi)
					.OnSelectionChanged(this, &SFootageIngestWidget::OnQueueListSelectionChanged)
					.ClearSelectionOnClick(true)
					.OnGenerateRow_Static(&SFootageQueuedTakeRow::BuildRow, StaticCastWeakPtr<SFootageIngestWidget>(AsWeak()))
				]
			]
		]
	];
}

void SFootageIngestWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SFootageIngestWidget::OnCurrentCaptureSourceChanged(TSharedPtr<FFootageCaptureSource> InCaptureSource, ESelectInfo::Type InSelectInfo)
{
	//when this method is invoked, the capture source has already been processed by CaptureSourcesWidget
	//it signals the change to the parent (CaptureManagerWidget), which then passes the signal here
	//so the Take View can be updated

	CurrentCaptureSource = InCaptureSource;

	if (!CurrentCaptureSource.IsValid()) {
		//empty the paths; the folder picker will be disabled, so no worries
		TargetFolderPickerAssetPath = FText::FromString("");
		TargetFolderPickerFullPathOnDisk = FText::FromString("");
	}
	else
	{
		// If the capture source already has a path set, use that one
		FString AssetPath;
		if (!CurrentCaptureSource->AssetPath.IsEmpty())
		{
			AssetPath = CurrentCaptureSource->AssetPath;
		}
		else
		{
			AssetPath = GetDefaultAssetPath(InCaptureSource->PackageName);
		}

		TargetFolderPickerAssetPath = FText::FromString(AssetPath);
		TargetFolderPickerFullPathOnDisk = FText::FromString(PathOnDiskFromAssetPath(AssetPath));
	}

	TargetFolderTextBox->SetText(TargetFolderPickerAssetPath); //show the full path with "[X]_Ingested" subfolder so the user knows where the files will actually go
	//do not allow picking a target path if there is no capture source selected
	PathPickerButton->SetEnabled(CurrentCaptureSource.IsValid());

	TakeSearchBar->SetText(FText::GetEmpty());
	SetTakeViewListSource(&GetCurrentTakeList());

	LoadAlreadyIngestedTakes(CurrentCaptureSource);
}

FString SFootageIngestWidget::PathOnDiskFromAssetPath(const FString& InAssetPath)
{
	FPackagePath PackagePath;
	FPackagePath::TryFromPackageName(InAssetPath, PackagePath);
	PackagePath.SetHeaderExtension(EPackageExtension::Asset);

	FString PackageFullPath = FPaths::ConvertRelativePathToFull(PackagePath.GetLocalFullPath());

	FString ParentPath, FileName, FileExtension;
	FPaths::Split(PackageFullPath, ParentPath, FileName, FileExtension);

	return ParentPath / FileName;
}

FString SFootageIngestWidget::GetDefaultAssetPath(const FName& InCaptureSourcePackageName)
{
	FString ParentPath, FileName, FileExtension;
	FPaths::Split(InCaptureSourcePackageName.ToString(), ParentPath, FileName, FileExtension);

	// Do not change the "_Ingested" bit below. This is a wildcard searched for to avoid auto import. See FMetaHumanCaptureSourceModule::StartupModule()
	FString TargetDir = FString::Format(TEXT("{0}/{1}_Ingested"), { ParentPath, FileName });

	return TargetDir;
}

void SFootageIngestWidget::SubscribeToCaptureSourceEvents(TSharedPtr<FFootageCaptureSource>& CaptureSource)
{
	CaptureSource->GetIngester().SubscribeToEvent(FTakeListResetEvent::Name,
		[Instance = AsWeak(), EventSource = CaptureSource.ToWeakPtr()](TSharedPtr<const FCaptureEvent>)
		{
			TSharedPtr<SFootageIngestWidget> SharedInstance = StaticCastSharedPtr<SFootageIngestWidget>(Instance.Pin());
			TSharedPtr<FFootageCaptureSource> SharedSource = EventSource.Pin();
			if (SharedInstance == nullptr || SharedSource == nullptr)
			{
				return;
			}

			SharedSource->TakeItems.Empty();
			if (SharedSource == SharedInstance->CurrentCaptureSource)
			{
				SharedInstance->TakeTileView->RebuildList();
			}

			SharedInstance->IngestedTakesCache.Empty();
		});

	CaptureSource->GetIngester().SubscribeToEvent(FNewTakesAddedEvent::Name,
		[Instance = AsWeak(), EventSource = CaptureSource.ToWeakPtr()](TSharedPtr<const FCaptureEvent> InEvent)
		{
			TSharedPtr<SFootageIngestWidget> SharedInstance = StaticCastSharedPtr<SFootageIngestWidget>(Instance.Pin());
			TSharedPtr<FFootageCaptureSource> SharedSource = EventSource.Pin();
			if (SharedInstance == nullptr || SharedSource == nullptr)
			{
				return;
			}

			const FNewTakesAddedEvent* NewTakesAddedEvent = static_cast<const FNewTakesAddedEvent*>(InEvent.Get());
			SharedInstance->UpdateTakeList(SharedSource, NewTakesAddedEvent->NewTakes);
		});

	CaptureSource->GetIngester().SubscribeToEvent(FThumbnailChangedEvent::Name,
		[Instance = AsWeak(), EventSource = CaptureSource.ToWeakPtr()](TSharedPtr<const FCaptureEvent> InEvent)
		{
			TSharedPtr<SFootageIngestWidget> SharedInstance = StaticCastSharedPtr<SFootageIngestWidget>(Instance.Pin());
			TSharedPtr<FFootageCaptureSource> SharedSource = EventSource.Pin();
			if (SharedInstance == nullptr || SharedSource == nullptr)
			{
				return;
			}

			const FThumbnailChangedEvent* ThumbnailChangedEvent = static_cast<const FThumbnailChangedEvent*>(InEvent.Get());
			SharedInstance->UpdateThumbnail(*SharedSource, ThumbnailChangedEvent->ChangedTake);
		});

	CaptureSource->GetIngester().SubscribeToEvent(FConnectionChangedEvent::Name,
										   [Instance = AsWeak(), EventSource = CaptureSource.ToWeakPtr()](TSharedPtr<const FCaptureEvent> InEvent)
	{
		TSharedPtr<SFootageIngestWidget> SharedInstance = StaticCastSharedPtr<SFootageIngestWidget>(Instance.Pin());
		TSharedPtr<FFootageCaptureSource> SharedSource = EventSource.Pin();
		if (SharedInstance == nullptr || SharedSource == nullptr)
		{
			return;
		}

		const FConnectionChangedEvent* ConnectionChangedEvent = static_cast<const FConnectionChangedEvent*>(InEvent.Get());

		switch (ConnectionChangedEvent->ConnectionState)
		{
			case FConnectionChangedEvent::EState::Disconnected:
				SharedSource->Status = EFootageCaptureSourceStatus::Offline;
				break;
			case FConnectionChangedEvent::EState::Connected:
				SharedSource->Status = EFootageCaptureSourceStatus::Online;
				break;
			case FConnectionChangedEvent::EState::Unknown:
			default:
				SharedSource->Status = EFootageCaptureSourceStatus::Closed;
				break;
		}
	});

	CaptureSource->GetIngester().SubscribeToEvent(FRecordingStatusChangedEvent::Name,
										   [Instance = AsWeak(), EventSource = CaptureSource.ToWeakPtr()](TSharedPtr<const FCaptureEvent> InEvent)
	{
		TSharedPtr<SFootageIngestWidget> SharedInstance = StaticCastSharedPtr<SFootageIngestWidget>(Instance.Pin());
		TSharedPtr<FFootageCaptureSource> SharedSource = EventSource.Pin();
		if (SharedInstance == nullptr || SharedSource == nullptr)
		{
			return;
		}

		const FRecordingStatusChangedEvent* RecordingStatusChangedEvent = static_cast<const FRecordingStatusChangedEvent*>(InEvent.Get());
		SharedSource->bIsRecording = RecordingStatusChangedEvent->bIsRecording;
	});

	CaptureSource->GetIngester().SubscribeToEvent(FTakesRemovedEvent::Name,
										   [Instance = AsWeak(), EventSource = CaptureSource.ToWeakPtr()](TSharedPtr<const FCaptureEvent> InEvent)
	{
		TSharedPtr<SFootageIngestWidget> SharedInstance = StaticCastSharedPtr<SFootageIngestWidget>(Instance.Pin());
		TSharedPtr<FFootageCaptureSource> SharedSource = EventSource.Pin();
		if (SharedInstance == nullptr || SharedSource == nullptr)
		{
			return;
		}

		const FTakesRemovedEvent* TakesRemovedEvent = static_cast<const FTakesRemovedEvent*>(InEvent.Get());
		SharedInstance->RemoveFromTakeList(SharedSource, TakesRemovedEvent->TakesRemoved);
	});
}

bool SFootageIngestWidget::CanClose()
{
	bool bIsImporting = false;

	for (TSharedPtr<FFootageCaptureSource>& CaptureSource : CaptureSources)
	{
		bIsImporting |= CaptureSource->bImporting;
	}

	if (bIsImporting)
	{
		FTextBuilder TextBuilder;
		TextBuilder.AppendLine(LOCTEXT("FootageIngestIsImportingDialog_Text", "Some of the takes are being imported and will be canceled."));
		TextBuilder.AppendLine(); // New line

		TextBuilder.AppendLine(LOCTEXT("FootageIngestIsImportingDialog_Takes", "Takes being imported:"));
		TextBuilder.Indent();

		for (TSharedPtr<FFootageTakeItem> QueuedTake : QueuedTakes)
		{
			if (QueuedTake->Status == EFootageTakeItemStatus::Ingest_Active)
			{
				TextBuilder.AppendLine(QueuedTake->Name);
			}
		}

		TextBuilder.Unindent();
		TextBuilder.AppendLine(); // New line
		TextBuilder.AppendLine(LOCTEXT("FootageIngestIsImportingDialog_Question", "Are you sure you want to continue?"));

		EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNo,
															 TextBuilder.ToText());

		return Response == EAppReturnType::Yes;
	}

	return true;
}

void SFootageIngestWidget::OnClose()
{
	// Unsubscribe from all event so we don't receive them while UI is not visible
	for (TSharedPtr<FFootageCaptureSource>& CaptureSource : CaptureSources)
	{
		ensureMsgf(CaptureSource, TEXT("Capture source is nullptr"));
		if (CaptureSource)
		{
			CaptureSource->GetIngester().UnsubscribeAll();
		}
	}
}

void SFootageIngestWidget::OnCaptureSourcesChanged(TArray<TSharedPtr<FFootageCaptureSource>> InCaptureSources)
{
	TArray<FFootageCaptureSource*> SourcesRemoved;
	for (TSharedPtr<FFootageCaptureSource>& CaptureSource : CaptureSources)
	{
		if (!InCaptureSources.Contains(CaptureSource))
		{
			SourcesRemoved.Add(CaptureSource.Get());
		}
	}

	if (!SourcesRemoved.IsEmpty())
	{
		TArray<TSharedPtr<FFootageTakeItem>> TakesToUnqueue;
		for (TSharedPtr<FFootageTakeItem> QueuedItem : QueuedTakes)
		{
			if (SourcesRemoved.Contains(QueuedItem->CaptureSource.Get()))
			{
				TakesToUnqueue.Add(QueuedItem);
			}
		}

		if (!TakesToUnqueue.IsEmpty())
		{
			UnqueueTakes(MoveTemp(TakesToUnqueue));

			if (QueuedTakes.IsEmpty())
			{
				bImportingTakes = false;
			}
		}
	}

	// If there are new sources, subscribe to events
	for (TSharedPtr<FFootageCaptureSource>& CaptureSource : InCaptureSources)
	{
		if (!CaptureSources.Contains(CaptureSource))
		{
			SubscribeToCaptureSourceEvents(CaptureSource);
		}
	}

	CaptureSources = MoveTemp(InCaptureSources);
	//nullifying the CurrentCaptureSource in case that it has been deleted is already handled by the CaptureSourcesWidget
	//which invokes this method, so no need to do any extra work here
}

void SFootageIngestWidget::OnCaptureSourceUpdated(TSharedPtr<FFootageCaptureSource> InCaptureSource)
{
	if (CaptureSources.Contains(InCaptureSource))
	{
		SubscribeToCaptureSourceEvents(InCaptureSource);

		UnqueueTakes(InCaptureSource->TakeItems);

		if (QueuedTakes.IsEmpty())
		{
			bImportingTakes = false;
		}

		InCaptureSource->TakeItems.Empty();

		if (CurrentCaptureSource == InCaptureSource)
		{
			TakeTileView->RebuildList();
		}
	}
}

void SFootageIngestWidget::OnTakeViewSelectionChanged(TSharedPtr<FFootageTakeItem> InTakeItem, ESelectInfo::Type InSelectInfo)
{
	if (InSelectInfo != ESelectInfo::Type::Direct && QueueListView.IsValid())
	{
		QueueListView->ClearSelection();
	}
}

void SFootageIngestWidget::OnQueueListSelectionChanged(TSharedPtr<FFootageTakeItem> InTakeItem, ESelectInfo::Type InSelectInfo)
{
	if (InSelectInfo != ESelectInfo::Type::Direct && TakeTileView.IsValid())
	{
		TakeTileView->ClearSelection();
	}
}

void SFootageIngestWidget::OnTakeFilterTextCommitted(const FText& InSearchText, ETextCommit::Type InCommitType)
{
	OnTakeFilterTextChanged(InSearchText);
}

void SFootageIngestWidget::OnTakeFilterTextChanged(const FText& InSearchText)
{
	TakeFilterText = InSearchText;

	if (TakeFilterText.IsEmpty())
	{
		SetTakeViewListSource(&GetCurrentTakeList());
	}
	else
	{
		TakeItems_Filtered.Empty();

		TArray<FString> ItemsToSearch;
		TakeFilterText.ToString().ParseIntoArray(ItemsToSearch, TEXT(" "));

		for (const FString& ItemToSearch : ItemsToSearch)
		{
			for (const TSharedPtr<FFootageTakeItem>& Item : GetCurrentTakeList())
			{
				if (Item->Name.ToString().Contains(ItemToSearch))
				{
					TakeItems_Filtered.AddUnique(Item);
				}
			}
		}

		SetTakeViewListSource(&TakeItems_Filtered);
	}

	TakeTileView->RebuildList();
}

FText SFootageIngestWidget::GetTakeCountText() const
{
	const int32 NumTakes = TakeViewListSource->Num();
	const int32 NumSelectedTakes = TakeTileView.IsValid() ? TakeTileView->GetNumItemsSelected() : 0;

	FText Result = FText::GetEmpty();

	if (NumSelectedTakes == 0)
	{
		if (NumTakes == 1)
		{
			Result = LOCTEXT("TakeCountLabelSingular", "1 Footage Item");
		}
		else
		{
			Result = FText::Format(LOCTEXT("TakeCountLabelPlural", "{0} Footage Items"), FText::AsNumber(NumTakes));
		}
	}
	else
	{
		if (NumTakes == 1)
		{
			Result = FText::Format(LOCTEXT("TakeCountLabelSingularPlusSelection", "1 Footage Item ({0} selected)"), FText::AsNumber(NumSelectedTakes));
		}
		else
		{
			Result = FText::Format(LOCTEXT("TakeCountLabelPluralPlusSelection", "{0} Footage Items ({1} selected)"), FText::AsNumber(NumTakes), FText::AsNumber(NumSelectedTakes));
		}
	}

	return Result;
}

FReply SFootageIngestWidget::OnImportTakesClicked()
{
	for (TSharedPtr<FFootageCaptureSource> Src : CaptureSources)
	{
		if (Src->GetIngester().CanIngestTakes())
		{
			TArray<TakeId> TakeIdsToImport = TArray<TakeId>();
			TArray<TSharedPtr<FFootageTakeItem>> TakesAlreadyImported;
			for (TSharedPtr<FFootageTakeItem> TakeItem : Src->TakeItems)
			{
				// TODO: Take status and ingest status should be two different attributes
				if (TakeItem->Status == EFootageTakeItemStatus::Queued ||
					TakeItem->Status == EFootageTakeItemStatus::Ingest_Failed ||
					TakeItem->Status == EFootageTakeItemStatus::Ingest_Canceled ||
					TakeItem->Status == EFootageTakeItemStatus::Ingest_Succeeded ||
					TakeItem->Status == EFootageTakeItemStatus::Ingest_Succeeded_with_Warnings)
				{
					if (CheckIfTakeShouldBeIngested(Src->Name.ToString(), TakeItem->TakeId))
					{
						TakesAlreadyImported.Add(TakeItem);
					}
					else
					{
						TakeIdsToImport.Emplace(TakeItem->TakeId);
						TakeItem->Status = EFootageTakeItemStatus::Ingest_Active;
					}
				}
			}

			if (PresentDialogForIngestedTakes(TakesAlreadyImported))
			{
				for (TSharedPtr<FFootageTakeItem> TakeItem : TakesAlreadyImported)
				{
					TakeIdsToImport.Emplace(TakeItem->TakeId);
					TakeItem->Status = EFootageTakeItemStatus::Ingest_Active;
				}
			}

			if (!TakeIdsToImport.IsEmpty())
			{
				using namespace UE::MetaHuman;
				FIngester::FGetTakesCallbackPerTake DelegatePerTake = FIngester::FGetTakesCallbackPerTake::Type::CreateSP(this, &SFootageIngestWidget::OnGetTakeImported);
				bool GetTakesStarted = Src->GetIngester().GetTakes(TakeIdsToImport, MoveTemp(DelegatePerTake));

				Src->bImporting = GetTakesStarted;
				bImportingTakes = GetTakesStarted;
			}
		}
	}

	return FReply::Handled();
}

void SFootageIngestWidget::OnGetTakeImported(FMetaHumanCapturePerTakeVoidResult InResult)
{
	const TakeId ImportedTakeId = InResult.TakeId;

	EFootageTakeItemStatus Status = EFootageTakeItemStatus::Ingest_Succeeded;
	FString StatusMessage;

	if (!InResult.Result.bIsValid)
	{
		if (InResult.Result.Code == EMetaHumanCaptureError::AbortedByUser)
		{
			Status = EFootageTakeItemStatus::Ingest_Canceled;
		}
		else if (InResult.Result.Code == EMetaHumanCaptureError::Warning)
		{
			Status = EFootageTakeItemStatus::Ingest_Succeeded_with_Warnings;
			StatusMessage = MoveTemp(InResult.Result.Message);
		}
		else
		{
			Status = EFootageTakeItemStatus::Ingest_Failed;
			StatusMessage = MoveTemp(InResult.Result.Message);
		}
	}

	for (TSharedPtr<FFootageTakeItem>& Item : QueuedTakes)
	{
		if (Item->TakeId == ImportedTakeId &&
			Status != EFootageTakeItemStatus::Ingest_Succeeded)
		{
			if (Status == EFootageTakeItemStatus::Ingest_Canceled)
			{
				UE_LOG(LogCaptureManager, Log, TEXT("Ingest for take %s was aborted by user"), *Item->Name.ToString());
			}
			else if (Status == EFootageTakeItemStatus::Ingest_Succeeded_with_Warnings)
			{
				UE_LOG(LogCaptureManager, Warning, TEXT("Ingest for take %s produced warnings: '%s'"), *Item->Name.ToString(), *StatusMessage);
			}
			else
			{
				UE_LOG(LogCaptureManager, Error, TEXT("Ingest for take %s failed: '%s'"), *Item->Name.ToString(), *StatusMessage);
			}
			Item->Status = Status;
			Item->StatusMessage = MoveTemp(StatusMessage);
		}
	}
}

bool SFootageIngestWidget::IsImportTakesEnabled() const
{
	return !bImportingTakes && !QueuedTakes.IsEmpty();
}

FReply SFootageIngestWidget::OnCancelAllImportClicked()
{
	for (TSharedPtr<FFootageCaptureSource> Src : CaptureSources)
	{
		if (Src->bImporting && Src->GetIngester().CanCancel())
		{
			TArray<int32> EmptyList; //passing an empty list will cancel all takes for the given source
			Src->GetIngester().CancelProcessing(EmptyList);
		}
	}

	return FReply::Handled();
}

FReply SFootageIngestWidget::OnClearAllImportClicked()
{
	bool bRefreshQueueView = false;

	if (QueueListView.IsValid())
	{
		//Clear all the (imported) takes from the list
		TArrayView<const TSharedPtr<FFootageTakeItem>> Takes = QueueListView->GetItems();

		bool bSomeFailed = false;
		for (TSharedPtr<FFootageTakeItem> Take : Takes)
		{
			if (Take->Status == EFootageTakeItemStatus::Ingest_Failed)
			{
				bSomeFailed = true;
				break;
			}
		}

		if (bSomeFailed)
		{
			if (EAppReturnType::Ok != FMessageDialog::Open(EAppMsgType::OkCancel,
				LOCTEXT("FootageIngestClearAllDialog", "Some of the takes failed to import.\nAre you sure you want to clear the list?")))
			{
				return FReply::Handled();
			}
		}

		TArray<TSharedPtr<FFootageTakeItem>> TakesToUnqueue;
		for (TSharedPtr<FFootageTakeItem> Take : Takes)
		{
			if (Take->Status == EFootageTakeItemStatus::Queued || 
				Take->Status == EFootageTakeItemStatus::Ingest_Succeeded || 
				Take->Status == EFootageTakeItemStatus::Ingest_Succeeded_with_Warnings ||
				Take->Status == EFootageTakeItemStatus::Ingest_Canceled ||
				Take->Status == EFootageTakeItemStatus::Ingest_Failed)
			{
				TakesToUnqueue.Add(Take);
			}
		}

		UnqueueTakes(MoveTemp(TakesToUnqueue));
	}

	return FReply::Handled();
}

bool SFootageIngestWidget::IsCancelAllImportEnabled() const
{
	return bImportingTakes;
}

bool SFootageIngestWidget::IsClearAllImportEnabled() const
{
	return !bImportingTakes && !QueuedTakes.IsEmpty();
}

void SFootageIngestWidget::SetDefaultAssetCreationPath(const FString& InDefaultAssetCreationPath)
{
	// Make sure there is a trailing slash, so we can use a simple String.StartsWith() to detect if a path lies within this folder
	DefaultAssetCreationPath = InDefaultAssetCreationPath / FString();
}

FReply SFootageIngestWidget::OnQueueButtonClicked()
{
	if (CurrentCaptureSource.IsValid())
	{
		// The target path strings (starting with /Game and with "/[CaptureSourceName]_Ingested" sufix) were memorized when CaptureSource was selected 
		// and then again if a path was picked in the Target Folder Picker; we now set that path in the CaptureSource
		// 
		// Make sure there is a trailing slash. The default asset creation path will have one, so we need to make sure that we have one here as well
		// for the subdirectory check below.
		const FString TargetPath = FPaths::GetPath(TargetFolderPickerAssetPath.ToString()) / FString();

		// Check that the target ingest location is within the project
		if (!TargetPath.StartsWith(DefaultAssetCreationPath))
		{
			const FText Message = LOCTEXT("IngestLocationOutsideProject", "Cannot ingest to read-only location outside of the current project.\nCurrent target is {0} when {1} is expected");

			// Ingest target is not in the current project which is not supported
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(Message, FText::FromString(TargetPath), FText::FromString(DefaultAssetCreationPath)));
			return FReply::Handled();
		}

		CurrentCaptureSource->GetIngester().SetTargetPath(TargetFolderPickerFullPathOnDisk.ToString(), TargetFolderPickerAssetPath.ToString());
		CurrentCaptureSource->AssetPath = TargetFolderPickerAssetPath.ToString();
	}

	bool bRefreshQueueView = false;

	if (TakeTileView.IsValid() && TakeTileView->GetNumItemsSelected() > 0)
	{
		TArray<TSharedPtr<FFootageTakeItem>> SelectedTakes = TakeTileView->GetSelectedItems();

		for (TSharedPtr<FFootageTakeItem> Take : SelectedTakes)
		{
			if (Take->Status == EFootageTakeItemStatus::Unqueued)
			{
				Take->Status = EFootageTakeItemStatus::Queued;
				Take->DestinationFolder = TargetFolderPickerAssetPath; //this is set in OnTargetPathChange
				QueuedTakes.Emplace(Take);
				bRefreshQueueView = true;
			}
			else if (Take->Status == EFootageTakeItemStatus::Queued) //already queued?
			{
				//refresh the paths in the item widgets
				Take->DestinationFolder = TargetFolderPickerAssetPath; //this is set in OnTargetPathChange
				bRefreshQueueView = true;
			}
		}
	}
	else if (QueueListView.IsValid() && QueueListView->GetNumItemsSelected() > 0)
	{
		TArray<TSharedPtr<FFootageTakeItem>> SelectedTakes = QueueListView->GetSelectedItems();

		for (TSharedPtr<FFootageTakeItem> Take : SelectedTakes)
		{
			// TODO allow cancellation of ingesting takes via this button?

			if (Take->Status == EFootageTakeItemStatus::Queued || 
				Take->Status == EFootageTakeItemStatus::Ingest_Succeeded ||
				Take->Status == EFootageTakeItemStatus::Ingest_Failed ||
				Take->Status == EFootageTakeItemStatus::Ingest_Canceled ||
				Take->Status == EFootageTakeItemStatus::Ingest_Succeeded_with_Warnings)
			{
				Take->Status = EFootageTakeItemStatus::Unqueued;
				Take->DestinationFolder = FText::GetEmpty();
				QueuedTakes.Remove(Take);
				bRefreshQueueView = true;
			}
		}
	}

	if (bRefreshQueueView)
	{
		QueueListView->RebuildList();
	}

	return FReply::Handled();
}

bool SFootageIngestWidget::IsQueueButtonEnabled() const
{
	bool bTakesSelected = (TakeTileView.IsValid() && TakeTileView->GetNumItemsSelected() > 0)
		|| (QueueListView.IsValid() && QueueListView->GetNumItemsSelected() > 0);
	return bTakesSelected && !bImportingTakes;
}

const FSlateBrush* SFootageIngestWidget::GetQueueButtonIcon() const
{
	if (QueueListView.IsValid() && QueueListView->GetNumItemsSelected() > 0)
	{
		return FAppStyle::Get().GetBrush("Icons.Minus");
	}

	return FAppStyle::Get().GetBrush("Icons.Plus");
}

FText SFootageIngestWidget::GetQueueButtonText() const
{
	if (QueueListView.IsValid() && QueueListView->GetNumItemsSelected() > 0)
	{
		return LOCTEXT("FootageIngestRemoveFromQueue", "Remove From Queue");
	}

	return LOCTEXT("FootageIngestAddToQueue", "Add To Queue");
}

FText SFootageIngestWidget::GetQueueButtonTooltip() const
{
	FText AddToQueueTooltip = LOCTEXT("AddToQueueToolTip", "Add selected take(s) to import queue.");
	if (IsCurrentCaptureSourceAssetValid())
	{
		if (QueueListView.IsValid() && QueueListView->GetNumItemsSelected() > 0)
		{
			return LOCTEXT("RemoveFromQueueToolTip", "Remove selected take(s) from import queue.");
		}

		if (TakeTileView.IsValid())
		{
			if (TakeTileView->GetNumItemsSelected() == 0)
			{
				return FText::Format(LOCTEXT("AddToQueueSelectTakesToolTip", "{0}\n\nTo enable this option, select some takes."), AddToQueueTooltip);
			}
			else
			{
				return AddToQueueTooltip;
			}
		}
		else
		{
			return FText();
		}
	}
	else
	{
		return FText::Format(LOCTEXT("AddToQueueSelectSourceToolTip", "{0}\n\nTo enable this option, please select a Capture Source."), AddToQueueTooltip);
	}
}

FText SFootageIngestWidget::GetTargetFolderPickerPathTooltip() const
{
	FText FolderPickerPathTooltip = LOCTEXT("TargetFolderPickerToolTip", "This is the path takes added to the queue will be imported to.");
	if (IsCurrentCaptureSourceAssetValid())
	{
		return FolderPickerPathTooltip;
	}
	else
	{
		return FText::Format(LOCTEXT("TargetFolderPickerSelectSourceToolTip", "{0}\n\nTo enable this option, please select a Capture Source"), FolderPickerPathTooltip);
	}

}

bool SFootageIngestWidget::IsCurrentCaptureSourceAssetValid() const
{
	return CurrentCaptureSource.IsValid();
}

void SFootageIngestWidget::UpdateTakeList(TSharedPtr<FFootageCaptureSource> InCaptureSource, const TArray<TakeId>& NewTakes)
{
	bool bRefreshTakeView = false;
	FMetaHumanTakeInfo TakeInfo;

	if (InCaptureSource)
	{
		// Load take tiles for all takes that haven't been loaded yet.
		for (TakeId NewTake : NewTakes)
		{
			if (InCaptureSource->GetIngester().GetTakeInfo(NewTake, TakeInfo))
			{
				TSharedPtr<FFootageTakeItem> TakeItem = MakeShared<FFootageTakeItem>();
				TakeItem->Name = FText::FromString(TakeInfo.Name);
				TakeItem->TakeId = TakeInfo.Id;
				TakeItem->NumFrames = TakeInfo.NumFrames;
				if (!TakeInfo.RawThumbnailData.IsEmpty())
				{
					LoadThumbnail(TakeInfo.RawThumbnailData, TakeItem);
				}

				TakeItem->Status = TakeInfo.Issues.IsEmpty() ? EFootageTakeItemStatus::Unqueued : EFootageTakeItemStatus::Warning;
				TakeItem->StatusMessage = !TakeInfo.Issues.IsEmpty() ? FText::Join(FText::FromString(TEXT("\n")), TakeInfo.Issues).ToString() : TEXT("");
				
				TakeItem->CaptureSource = InCaptureSource;

				InCaptureSource->TakeItems.Emplace(TakeItem);

				CheckIfTakeIsAlreadyIngested(TakeItem);

				bRefreshTakeView = true;
			}
		}

		InCaptureSource->TakeItems.Sort([CaptureSource = InCaptureSource.Get()](const TSharedPtr<FFootageTakeItem>& InLeft, const TSharedPtr<FFootageTakeItem>& InRight)
			{
				FMetaHumanTakeInfo LeftTakeInfo;
				CaptureSource->GetIngester().GetTakeInfo(InLeft->TakeId, LeftTakeInfo);

				FMetaHumanTakeInfo RightTakeInfo;
				CaptureSource->GetIngester().GetTakeInfo(InRight->TakeId, RightTakeInfo);

				if (LeftTakeInfo.Date.GetTicks() == RightTakeInfo.Date.GetTicks())
				{
					return InLeft->Name.ToString() < InRight->Name.ToString();
				}

				return LeftTakeInfo.Date.GetTicks() > RightTakeInfo.Date.GetTicks();
			});

		if (InCaptureSource == CurrentCaptureSource && bRefreshTakeView)
		{
			for (const TSharedPtr<FFootageTakeItem>& TakeItem : CurrentCaptureSource->TakeItems)
			{
				if (!TakeFilterText.IsEmpty())
				{
					if (TakeItem->Name.ToString().Contains(TakeFilterText.ToString()))
					{
						TakeItems_Filtered.Emplace(TakeItem);
					}
				}
			}

			// If anything changed, refresh the take view.
			TakeTileView->RebuildList();
		}
	}
}

void SFootageIngestWidget::RemoveFromTakeList(TSharedPtr<FFootageCaptureSource> InCaptureSource, const TArray<TakeId>& RemovedTakes)
{
	if (InCaptureSource)
	{
		bool bRefreshTakeView = false;

		for (TakeId RemovedTakeId : RemovedTakes)
		{
			int32 Index = InCaptureSource->TakeItems.IndexOfByPredicate([RemovedTakeId](const TSharedPtr<FFootageTakeItem>& InElem)
			{
				return InElem->TakeId == RemovedTakeId;
			});

			if (Index == INDEX_NONE)
			{
				return;
			}

			// Unqueue take - it will be canceled internally
			UnqueueTake(InCaptureSource->TakeItems[Index], true);

			// Removed it from already ingested takes
			FString Name = InCaptureSource->Name.ToString();
			if (IngestedTakesCache.Contains(Name))
			{
				IngestedTakesCache[Name].Remove(Index);
			}

			// Remove it from the list of takes
			InCaptureSource->TakeItems.RemoveAt(Index);

			bRefreshTakeView = true;
		}

		InCaptureSource->TakeItems.Sort([CaptureSource = InCaptureSource.Get()](const TSharedPtr<FFootageTakeItem>& InLeft, const TSharedPtr<FFootageTakeItem>& InRight)
		{
			FMetaHumanTakeInfo LeftTakeInfo;
			CaptureSource->GetIngester().GetTakeInfo(InLeft->TakeId, LeftTakeInfo);

			FMetaHumanTakeInfo RightTakeInfo;
			CaptureSource->GetIngester().GetTakeInfo(InRight->TakeId, RightTakeInfo);
			return LeftTakeInfo.Date.GetTicks() > RightTakeInfo.Date.GetTicks();
		});

		if (InCaptureSource == CurrentCaptureSource && bRefreshTakeView)
		{
			for (const TSharedPtr<FFootageTakeItem>& TakeItem : CurrentCaptureSource->TakeItems)
			{
				if (!TakeFilterText.IsEmpty())
				{
					if (TakeItem->Name.ToString().Contains(TakeFilterText.ToString()))
					{
						TakeItems_Filtered.Emplace(TakeItem);
					}
				}
			}

			// If anything changed, refresh the take view.
			TakeTileView->RebuildList();
		}
	}
}

TArray<TSharedPtr<FFootageTakeItem>>& SFootageIngestWidget::GetCurrentTakeList()
{
	return CurrentCaptureSource.IsValid() ? CurrentCaptureSource->TakeItems : TakeItems_Null;
}

void SFootageIngestWidget::SetTakeViewListSource(TArray<TSharedPtr<FFootageTakeItem>>* InListSource)
{
	TakeViewListSource = InListSource;
	TakeTileView->SetItemsSource(TakeViewListSource);
}

void SFootageIngestWidget::OnCaptureSourceFinishedImportingTakes(const TArray<FMetaHumanTake>& InTakes, TSharedPtr<FFootageCaptureSource> InCaptureSource)
{
	// The take asset goes into the TargetFolderAssetPath the user has picked; in case nothing is picked, the takes go to the folder with CaptureSource),
	//while the associated data goes to sub-folders named after each take

	TMap<int32, FString> CaptureDataFailedTakes;
	for (const FMetaHumanTake& Take : InTakes)
	{
		FMetaHumanTakeInfo TakeInfo;
		if (InCaptureSource->GetIngester().GetTakeInfo(Take.TakeId, TakeInfo))
		{
			if (UFootageCaptureData* CaptureData = GetOrCreateCaptureData(InCaptureSource->AssetPath, TakeInfo.Name))
			{
				CaptureData->ImageSequences.Reset();
				CaptureData->DepthSequences.Reset();
#if WITH_EDITOR
				for (const FMetaHumanTakeView& TakeView : Take.Views)
				{
					if (TakeView.bVideoTimecodePresent)
					{
						UImageSequenceTimecodeUtils::SetTimecodeInfo(TakeView.VideoTimecode, TakeView.VideoTimecodeRate, TakeView.Video.Get());
					}
					CaptureData->ImageSequences.Add(TakeView.Video);

					if (TakeView.bDepthTimecodePresent)
					{
						UImageSequenceTimecodeUtils::SetTimecodeInfo(TakeView.DepthTimecode, TakeView.DepthTimecodeRate, TakeView.Depth.Get());
					}
					CaptureData->DepthSequences.Add(TakeView.Depth);
				}
#endif
				CaptureData->CameraCalibrations.Reset();
				CaptureData->CameraCalibrations.Add(Take.CameraCalibration);

				if (Take.Audio)
				{
					CaptureData->AudioTracks.Reset();
					CaptureData->AudioTracks.Add(Take.Audio);
				}

				CaptureData->Metadata.FrameRate = TakeInfo.FrameRate;
				CaptureData->Metadata.DeviceModelName = TakeInfo.DeviceModel;
				CaptureData->Metadata.SetDeviceClass(TakeInfo.DeviceModel);
				CaptureData->CaptureExcludedFrames = Take.CaptureExcludedFrames;

				TArray<FAssetData> AssetsInPath = GetAssetsInPath(InCaptureSource->AssetPath / TakeInfo.OutputDirectory);

				AssetsToSave.Append(AssetsInPath);
				AssetsToSave.Add(CaptureData);
			}
			else
			{
				FText Message = LOCTEXT("IngestError_CaptureDataCreation", "Failed to create Capture Data (Footage)");
				CaptureDataFailedTakes.Add(TakeInfo.Id, Message.ToString());
			}
		}
	}

	InCaptureSource->bImporting = false;

	// Update statuses of take items.
	for (TSharedPtr<FFootageTakeItem> Take : InCaptureSource->TakeItems)
	{
		if (CaptureDataFailedTakes.Contains(Take->TakeId))
		{
			Take->Status = EFootageTakeItemStatus::Ingest_Failed;
			Take->StatusMessage = CaptureDataFailedTakes[Take->TakeId];
		}
		else
		{
			if (Take->Status == EFootageTakeItemStatus::Ingest_Active)
			{
				Take->Status = EFootageTakeItemStatus::Ingest_Succeeded;

				FString Name = InCaptureSource->Name.ToString();
				if (!IngestedTakesCache.Contains(Name))
				{
					IngestedTakesCache.Emplace(Name);
				}
				IngestedTakesCache[Name].AddUnique(Take->TakeId);
			}
		}
	}

	QueueListView->RebuildList();

	// Only unset the global bImportingTakes if all sources are done importing.
	bImportingTakes = false;
	for (TSharedPtr<FFootageCaptureSource> Src : CaptureSources)
	{
		bImportingTakes |= Src->bImporting;
	}

	if (bSaveAfterIngest)
	{
		SaveImportedAssets();
	}
}

UFootageCaptureData* SFootageIngestWidget::GetOrCreateCaptureData(const FString& InTargetIngestPath, const FString& InAssetName) const
{
	UFootageCaptureData* FoundAsset = GetCaptureData(InTargetIngestPath, InAssetName);

	if (FoundAsset == nullptr)
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		FoundAsset = Cast<UFootageCaptureData>(AssetTools.CreateAsset(InAssetName, InTargetIngestPath, UFootageCaptureData::StaticClass(), nullptr));
	}
	
	return FoundAsset;
}

UFootageCaptureData* SFootageIngestWidget::GetCaptureData(const FString& InTargetIngestPath, const FString& InAssetName) const
{
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	const FString AssetPackagePath = InTargetIngestPath / InAssetName;

	TArray<FAssetData> AssetData;
	AssetRegistry.GetAssetsByPackageName(FName{ *AssetPackagePath }, AssetData);
	
	if (AssetData.IsEmpty())
	{
		return nullptr;
	}
	
	return Cast<UFootageCaptureData>(AssetData[0].GetAsset());
}

TArray<FAssetData> SFootageIngestWidget::GetAssetsInPath(const FString& InTargetIngestPath)
{
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> AssetsData;
	AssetRegistry.GetAssetsByPath(FName{ *InTargetIngestPath }, AssetsData, true, false);

	return AssetsData;
}

void SFootageIngestWidget::SaveImportedAssets()
{
	if (AssetsToSave.IsEmpty())
	{
		return;
	}

	TArray<UPackage*> Packages;
	for (const FAssetData& AssetData : AssetsToSave)
	{
		UPackage* Package = AssetData.GetAsset()->GetPackage();
		if (!Packages.Contains(Package))
		{
			Packages.Add(Package);
		}
	}

	UEditorLoadingAndSavingUtils::SavePackages(Packages, true);

	AssetsToSave.Empty();
}

bool SFootageIngestWidget::UnqueueTake(TSharedPtr<FFootageTakeItem> Take, bool bCancelingSingleItem)
{
	bool bRefreshQueueView = false;

	if (Take->Status != EFootageTakeItemStatus::Unqueued)
	{
		Take->Status = EFootageTakeItemStatus::Unqueued;
		Take->DestinationFolder = FText::GetEmpty();
		QueuedTakes.Remove(Take);
		bRefreshQueueView = true;
	}

	if (bCancelingSingleItem && bRefreshQueueView)
	{
		QueueListView->RebuildList();
	}

	return bRefreshQueueView;
}

void SFootageIngestWidget::UnqueueTakes(TArray<TSharedPtr<FFootageTakeItem>> Takes)
{
	bool bRefreshQueueView = false;

	// Update statuses of take items.
	for (TSharedPtr<FFootageTakeItem> Take : Takes)
	{
		bRefreshQueueView = UnqueueTake(Take, false) || bRefreshQueueView;
	}

	if (bRefreshQueueView)
	{
		QueueListView->RebuildList();
	}
}

#ifdef TARGET_PATH_PICKER

TSharedRef<SWidget> SFootageIngestWidget::GetPathPickerContent()
{
	FPathPickerConfig PathPickerConfig;
	FString PathWithoutIngestedSufix = TargetFolderPickerAssetPath.ToString();
	//"[CaptureSourceName]_Ingested" is a default path when the Capture Source is selected, and it is added as a suffix to whatever path the user picks in the target picker
	// It serves to prevent auto-import of the image files bundled inside it ("*_Ingested*" wildcard)
	// However, we don't want the user to be able to pick a folder with the [CaptureSourceName]_Ingested suffix directly, as a new _Ingested subfolders would be created automatically inside it.
	// As the TargetFolderAssetPath contains "[CaptureSourceName]_Ingested", we remove the suffix first before opening the path picker, so the correct parent folder is pre-selected for the user
	PathWithoutIngestedSufix.RemoveFromEnd(FString::Format(TEXT("{0}_Ingested"), { CurrentCaptureSource->Name.ToString() }));

	//the path picker button is disabled if CurrentCaptureSource is not selected, so we can safely use the source, and we also know that the TargetAssetFolderPath is set
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SFootageIngestWidget::OnTargetPathChange);
	PathPickerConfig.DefaultPath = PathWithoutIngestedSufix; //open the picker on the current path (CaptureSource folder by default)
	PathPickerConfig.bAddDefaultPath = false; //since the default path is the path to the current CaptureSource, it surely exists; this flag is do not add it if it doesn't
	PathPickerConfig.bAllowContextMenu = true;
	PathPickerConfig.bAllowClassesFolder = false;
	PathPickerConfig.bOnPathSelectedPassesVirtualPaths = false; //ensures we don't have "/All" prefix in the paths that the picker returns; they will start with "/Game" instead
	PathPickerConfig.bAllowReadOnlyFolders = false;
	PathPickerConfig.bFocusSearchBoxWhenOpened = true;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	return SNew(SBox)
		.WidthOverride(300)
		.HeightOverride(500)
		.Padding(4)
		[
			SNew(SVerticalBox)

			// Path Picker
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
		]
		];
}

void SFootageIngestWidget::OnTargetPathChange(const FString& NewPath)
{
	//we just memorize the chosen path from Content folder here, but don't set it in the CaptureSource yet
	//we don't want to be creating empty folders and adding them to non auto-import list whenever the user picks a path,
	//so that's postponed until the actual ingestion

	//bOnPathSelectedPassesVirtualPaths in FPathPickerConfig needs to be false, otherwise NewPath will contain "/All" prefix

	const FString IngestPackagePath = FString::Format(TEXT("{0}/{1}_Ingested"), { NewPath, CurrentCaptureSource->Name.ToString() });

	TargetFolderPickerAssetPath = FText::FromString(IngestPackagePath);
	TargetFolderPickerFullPathOnDisk = FText::FromString(PathOnDiskFromAssetPath(IngestPackagePath));

	if (CurrentCaptureSource)
	{
		CurrentCaptureSource->AssetPath = IngestPackagePath;
	}

	//display in the text field
	TargetFolderTextBox->SetText(TargetFolderPickerAssetPath);

	OnTargetFolderAssetPathChangedDelegate.ExecuteIfBound(TargetFolderPickerAssetPath);
}

#endif// TARGET_PATH_PICKER

// Note: We no longer use this function. We use FMetaHumanCaptureSourceModule::PostEngineInit instead.
// We will deprecate this function in 5.7 but are holding off for now to prevent an ABI breaking change.
void SFootageIngestWidget::AddAutoReimportExemption(UEditorLoadingSavingSettings* Settings, FString DirectoryPath)
{
	// Add an exemption for files this module will create to the auto import setting. This places a restriction of
	// the name of the directory we can use for import
	FAutoReimportDirectoryConfig DirectoryConfig;
	DirectoryConfig.SourceDirectory = DirectoryPath.Left(DirectoryPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1) + 1); // Everything up to the 2nd slash /Game/dir1/dir2 will be /Game/
	FAutoReimportWildcard Wildcard;
	DirectoryPath.RemoveFromStart(DirectoryConfig.SourceDirectory); //Directory path contains "/Game/" at the beginning
	//quick-fixing the auto-reimport issue by reintroducing _Ingested folder;
	//Wildcard.Wildcard = DirectoryPath+"/*"; //leaving this in the comment for future reference
	Wildcard.Wildcard = "*_Ingested/*";
	Wildcard.bInclude = false;
	DirectoryConfig.Wildcards.Add(Wildcard);
	bool bSettingPresent = false;
	for (int32 ConfigIndex = 0; ConfigIndex < Settings->AutoReimportDirectorySettings.Num() && !bSettingPresent; ++ConfigIndex)
	{
		const FAutoReimportDirectoryConfig& Config = Settings->AutoReimportDirectorySettings[ConfigIndex];
		bSettingPresent = (Config.SourceDirectory == DirectoryConfig.SourceDirectory &&
			Config.MountPoint == DirectoryConfig.MountPoint &&
			Config.Wildcards.Num() == DirectoryConfig.Wildcards.Num() &&
			Config.Wildcards[0].Wildcard == DirectoryConfig.Wildcards[0].Wildcard &&
			Config.Wildcards[0].bInclude == DirectoryConfig.Wildcards[0].bInclude);
	}
	if (!bSettingPresent)
	{
		Settings->AutoReimportDirectorySettings.Add(DirectoryConfig);
		Settings->SaveConfig();
		Settings->OnSettingChanged().Broadcast(GET_MEMBER_NAME_CHECKED(UEditorLoadingSavingSettings, AutoReimportDirectorySettings));
	}
}

void SFootageIngestWidget::UpdateThumbnail(FFootageCaptureSource& InCaptureSource, TakeId InTakeId)
{
	TSharedPtr<FFootageTakeItem> TakeItem = GetTakeItemById(InCaptureSource, InTakeId);
	if (TakeItem == nullptr)
	{
		// Take list has been cleared since the event was emitted, so we're skipping it
		return;
	}

	FMetaHumanTakeInfo TakeInfo;
	if (InCaptureSource.GetIngester().GetTakeInfo(TakeItem->TakeId, TakeInfo))
	{
		if (!TakeInfo.RawThumbnailData.IsEmpty())
		{
			if (LoadThumbnail(TakeInfo.RawThumbnailData, TakeItem))
			{
				if (&InCaptureSource == CurrentCaptureSource.Get())
				{
					TakeTileView->RebuildList();
				}
			}
		}
	}
}

TSharedPtr<FFootageTakeItem> SFootageIngestWidget::GetTakeItemById(FFootageCaptureSource& InCaptureSource, TakeId InTakeId)
{
	for (int32 i = 0; i < InCaptureSource.TakeItems.Num(); i++)
	{
		if (InCaptureSource.TakeItems[i]->TakeId == InTakeId)
		{
			return InCaptureSource.TakeItems[i];
		}
	}
	return nullptr;
}

bool SFootageIngestWidget::LoadThumbnail(const TArray<uint8>& InThumbnailRawData, TSharedPtr<FFootageTakeItem> InTakeItem)
{
	UTexture2D* PreviewImageTexture = FImageUtils::ImportBufferAsTexture2D(InThumbnailRawData);
	if (PreviewImageTexture)
	{
		InTakeItem->PreviewImage = MakeShared<FSlateImageBrush>((UObject*)PreviewImageTexture, FVector2D(PreviewImageTexture->GetSizeX(), PreviewImageTexture->GetSizeY()));
		InTakeItem->PreviewSet = true;

		InTakeItem->PreviewImageTexture = PreviewImageTexture;

		return true;
	}

	return false;
}

bool SFootageIngestWidget::CheckIfTakeShouldBeIngested(const FString& InSourceName, const TakeId InTakeId) const
{
	if (IngestedTakesCache.Contains(InSourceName))
	{
		const TArray<TakeId> IngestedTakesForSource = IngestedTakesCache[InSourceName];

		return IngestedTakesForSource.Contains(InTakeId);
	}

	return false;
}

bool SFootageIngestWidget::PresentDialogForIngestedTakes(const TArray<TSharedPtr<FFootageTakeItem>>& InAlreadyIngestedTakes) const
{
	if (InAlreadyIngestedTakes.IsEmpty())
	{
		return false;
	}

	FTextBuilder TextBuilder;
	TextBuilder.AppendLine(LOCTEXT("FootageIngestAlreadyIngestedDialog_Text", "Some of the takes selected for import are already imported and will be overwritten."));
	TextBuilder.AppendLine(); // New line

	TextBuilder.AppendLine(LOCTEXT("FootageIngestAlreadyIngestedDialog_Takes", "Already imported takes:"));
	TextBuilder.Indent();

	for (const TSharedPtr<FFootageTakeItem>& AlreadyIngestedTake : InAlreadyIngestedTakes)
	{	
		TextBuilder.AppendLine(AlreadyIngestedTake->Name);
	}

	TextBuilder.Unindent();
	TextBuilder.AppendLine(); // New line
	TextBuilder.AppendLine(LOCTEXT("FootageIngestAlreadyIngestedDialog_Question", "Are you sure you want to continue?"));

	EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNo,
														 TextBuilder.ToText());

	return Response == EAppReturnType::Yes;
}

void SFootageIngestWidget::LoadAlreadyIngestedTakes(const TSharedPtr<FFootageCaptureSource> InCaptureSource)
{
	if (InCaptureSource.IsValid())
	{
		for (const TSharedPtr<FFootageTakeItem>& Take : InCaptureSource->TakeItems)
		{
			CheckIfTakeIsAlreadyIngested(Take);
		}
	}
}

void SFootageIngestWidget::CheckIfTakeIsAlreadyIngested(const TSharedPtr<FFootageTakeItem> InTake)
{
	if (InTake.IsValid())
	{
		if (GetCaptureData(InTake->CaptureSource->AssetPath, InTake->Name.ToString()) == nullptr)
		{
			return;
		}

		FString Name = InTake->CaptureSource->Name.ToString();
		if (!IngestedTakesCache.Contains(Name))
		{
			IngestedTakesCache.Add(Name);
		}

		IngestedTakesCache[Name].AddUnique(InTake->TakeId);
	}
}

#undef LOCTEXT_NAMESPACE

