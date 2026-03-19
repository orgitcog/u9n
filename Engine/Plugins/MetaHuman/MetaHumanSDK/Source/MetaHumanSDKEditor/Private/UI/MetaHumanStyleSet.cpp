// Copyright Epic Games, Inc. All Rights Reserved.
#include "UI/MetaHumanStyleSet.h"

#include "SAssetGroupItemView.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/StyleColors.h"

namespace UE::MetaHuman
{
FMetaHumanStyleSet& FMetaHumanStyleSet::Get()
{
	static FMetaHumanStyleSet TheInstance;
	return TheInstance;
}

FMetaHumanStyleSet::FMetaHumanStyleSet()
	: FSlateStyleSet(TEXT("FMetaHumanStyleSet"))
{
	FSlateStyleSet::SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
	FSlateStyleSet::SetContentRoot(IPluginManager::Get().FindPlugin("MetaHumanSDK")->GetContentDir());
	// Useful Constants
	constexpr float CornerRadius = 4.f;

	// "New" style tables with rounded edges
	FSlateRoundedBoxBrush RoundedHeaderBrush = FSlateRoundedBoxBrush(FStyleColors::Dropdown, CornerRadius);
	FSlateRoundedBoxBrush RoundedBackgroundBrush = FSlateRoundedBoxBrush(FStyleColors::Recessed, CornerRadius);
	FSlateRoundedBoxBrush HalfRoundedHeaderBrush = FSlateRoundedBoxBrush(FStyleColors::Dropdown, FVector4(CornerRadius, CornerRadius, 0.f, 0.f));

	FTableViewStyle TreeViewStyle = FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("TreeView");
	// Using a header color brush for the tree view's background gives the header square edges when there are child
	// items while still keeping it rounded when fully collapsed. This will look wrong if we ever make the tree view
	// expand to fill space and will need another solution.
	TreeViewStyle.SetBackgroundBrush(RoundedHeaderBrush);

	FTableViewStyle ListViewStyle = FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("ListView");
	ListViewStyle.SetBackgroundBrush(RoundedBackgroundBrush);

	FHeaderRowStyle ListHeaderRowStyle = FAppStyle::Get().GetWidgetStyle<FHeaderRowStyle>("TableView.Header");
	ListHeaderRowStyle.SetBackgroundBrush(HalfRoundedHeaderBrush);
	ListHeaderRowStyle.ColumnStyle.SetNormalBrush(HalfRoundedHeaderBrush);
	ListHeaderRowStyle.LastColumnStyle.SetNormalBrush(HalfRoundedHeaderBrush);

	FHeaderRowStyle TreeHeaderRowStyle = FAppStyle::Get().GetWidgetStyle<FHeaderRowStyle>("TableView.Header");
	TreeHeaderRowStyle.SetHorizontalSeparatorThickness(0);
	TreeHeaderRowStyle.SetHorizontalSeparatorBrush(FSlateNoResource());
	TreeHeaderRowStyle.SetBackgroundBrush(RoundedHeaderBrush);
	TreeHeaderRowStyle.ColumnStyle.SetNormalBrush(RoundedHeaderBrush);
	TreeHeaderRowStyle.LastColumnStyle.SetNormalBrush(RoundedHeaderBrush);

	FTableRowStyle TreeViewItemStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
	TreeViewItemStyle.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::Recessed));
	TreeViewItemStyle.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::Recessed));
	TreeViewItemStyle.SetActiveBrush(FSlateColorBrush(FStyleColors::Recessed));
	TreeViewItemStyle.SetActiveHoveredBrush(FSlateColorBrush(FStyleColors::Recessed));
	TreeViewItemStyle.SetInactiveBrush(FSlateColorBrush(FStyleColors::Recessed));
	TreeViewItemStyle.SetInactiveHoveredBrush(FSlateColorBrush(FStyleColors::Recessed));

	// Set up the style
	Set("MenuIcon", new IMAGE_BRUSH_SVG(TEXT("MetaHuman"), CoreStyleConstants::Icon16x16));

	// MetaHuman Manager
	Set("MetaHumanManager.TreeViewStyle", TreeViewStyle);
	Set("MetaHumanManager.ListViewStyle", ListViewStyle);
	Set("MetaHumanManager.ListHeaderRowStyle", ListHeaderRowStyle);
	Set("MetaHumanManager.TreeHeaderRowStyle", TreeHeaderRowStyle);
	Set("MetaHumanManager.TreeViewItemStyle", TreeViewItemStyle);
	Set("MetaHumanManager.RoundedBorder", new FSlateRoundedBoxBrush(FStyleColors::Panel, 4.f));
	Set("MetaHumanManager.IconMargin", FMargin{0.f, 0.f, 4.f, 0.f});
	Set("MetaHumanManager.NoIconMargin", FMargin{0.f, 0.f, 20.f, 0.f});

	// Layout variables
	Set("MetaHumanManager.WindowSize", FVector2D(670, 770));
	Set("MetaHumanManager.WindowMinHeight", 300.f);
	Set("MetaHumanManager.WindowMinWidth", 300.f);
	Set("MetaHumanManager.NavigationWidth", 192.f);
	Set("MetaHumanManager.ItemViewPadding", FMargin{2.f, 0.f, 0.f, 0.f});

	// Item Navigation styling
	Set("ItemNavigation.BorderPadding", 1.f);
	Set("ItemNavigation.HeaderPadding", 8.f);
	Set("ItemNavigation.HeaderFont", FCoreStyle::GetDefaultFontStyle("Bold", 10));
	Set("ItemNavigation.ListItemFont", FCoreStyle::GetDefaultFontStyle("Normal", 10));
	Set("ItemNavigation.ListItemMargin", FMargin{6.f, 2.f, 2.f, 2.f});

	// Item Viewport Styling
	Set("ItemDetails.MaximizeIcon", new IMAGE_BRUSH_SVG(TEXT("ThumbnailMaximize"), CoreStyleConstants::Icon16x16));
	Set("ItemDetails.MinimizeIcon", new IMAGE_BRUSH_SVG(TEXT("ThumbnailMinimize"), CoreStyleConstants::Icon16x16));
	Set("ItemDetails.CharacterIcon", new IMAGE_BRUSH_SVG(TEXT("Character"), CoreStyleConstants::Icon16x16));
	Set("ItemDetails.ClothingIcon", new IMAGE_BRUSH_SVG(TEXT("Clothing"), CoreStyleConstants::Icon16x16));
	Set("ItemDetails.GroomIcon", new IMAGE_BRUSH_SVG(TEXT("Groom"), CoreStyleConstants::Icon16x16));
	Set("ItemDetails.DefaultIcon", new IMAGE_BRUSH_SVG(TEXT("MetaHuman"), CoreStyleConstants::Icon16x16));
	Set("ItemDetails.DetailFileIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Common/file", CoreStyleConstants::Icon16x16));
	Set("ItemDetails.ThumbnailBorder", new FSlateRoundedBoxBrush(FStyleColors::Secondary, 2.f));
	Set("ItemDetails.ThumbnailInnerBorder", new FSlateRoundedBoxBrush(FStyleColors::Background, 2.f));
	Set("ItemDetails.Padding", FMargin{12.f, 12.f, 12.f, 0.f});
	Set("ItemDetails.ResizeButtonMargin", 6.f);
	Set("ItemDetails.ResizeButtonPadding", 2.f);
	Set("ItemDetails.SmallThumbnailSize", 200.f);
	Set("ItemDetails.LargeThumbnailSize", 450.f);
	Set("ItemDetails.DetailsSectionMargin", FMargin{0.f, 0.f, 0.f, 12.f});
	Set("ItemDetails.TitleFont", FCoreStyle::GetDefaultFontStyle("Bold", 12));
	Set("ItemDetails.DetailsTextFont", FCoreStyle::GetDefaultFontStyle("Normal", 10));
	Set("ItemDetails.DetailsEmphasisFont", FCoreStyle::GetDefaultFontStyle("Bold", 10));
	Set("ItemDetails.TitleTextMargin", FMargin{0.f, 2.f});
	Set("ItemDetails.TitleIconMargin", FMargin{0.f, 0.f, 6.f, 0.f});
	Set("ItemDetails.DetailRowPadding", 6.f);
	Set("ItemDetails.DetailsTextMargin", FMargin{0.f, 0.f, 0.f, 6.f});
	Set("ItemDetails.DetailColumnMargin", FMargin{8.f, 0.f, 8.f, 0.f});
	Set("ItemDetails.DetailEntryFont", FCoreStyle::GetDefaultFontStyle("Normal", 10));
	Set("ItemDetails.PackageButtonPadding", FMargin{0.f, 6.f, 4.f, 6.f});
	Set("ItemDetails.VerifyButtonPadding", FMargin{4.f, 6.f, 0.f, 6.f});

	// Report View Styling
	Set("ReportView.NoReportIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Common/x-circle", CoreStyleConstants::Icon16x16, FStyleColors::AccentGray));
	Set("ReportView.ErrorIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Common/x-circle", CoreStyleConstants::Icon16x16, FStyleColors::Error));
	Set("ReportView.WarningIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check-circle-solid", CoreStyleConstants::Icon16x16, FStyleColors::Warning));
	Set("ReportView.SuccessIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check-circle-solid", CoreStyleConstants::Icon16x16, FStyleColors::Success));
	Set("ReportView.BulletIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Common/bullet-point16", CoreStyleConstants::Icon16x16));
	Set("ReportView.HeaderPadding", FMargin(6.f));
	Set("ReportView.HeaderFont", FCoreStyle::GetDefaultFontStyle("Normal", 10));
	Set("ReportView.EntryFont", FCoreStyle::GetDefaultFontStyle("Normal", 10));
	Set("ReportView.SectionPadding", FMargin{5.f});
	Set("ReportView.EntryPadding", FMargin{10.f, 2.f, 2.f, 2.f});
}
} // namespace UE::MetaHuman
