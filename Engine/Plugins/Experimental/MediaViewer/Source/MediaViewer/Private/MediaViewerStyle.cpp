// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaViewerStyle.h"

#include "DetailLayoutBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "Styling/ToolBarStyle.h"

namespace UE::MediaViewer::Private
{

const FName FMediaViewerStyle::StyleName("MediaViewerStyle");

FMediaViewerStyle::FMediaViewerStyle()
	: FSlateStyleSet(StyleName)
{
	FSlateStyleSet::SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	FSlateStyleSet::SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	FTextBlockStyle NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	NormalText.SetFont(IDetailLayoutBuilder::GetDetailFont());
	NormalText.SetShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor());
	NormalText.SetShadowOffset(FVector2D(1.0, 1.0));
	Set("RichTextBlock.Normal", NormalText);
	Set("RichTextBlock.Red", FTextBlockStyle(NormalText).SetColorAndOpacity(FLinearColor(1.0f, 0.1f, 0.1f)));
	Set("RichTextBlock.Green", FTextBlockStyle(NormalText).SetColorAndOpacity(FLinearColor(0.1f, 1.0f, 0.1f)));
	Set("RichTextBlock.Blue", FTextBlockStyle(NormalText).SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 1.0f)));

	FSlateBrush* BrushTableRowOdd = new FSlateBrush();
	BrushTableRowOdd->TintColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
	Set("TableRowOdd", BrushTableRowOdd);

	FButtonStyle LibraryButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	LibraryButtonStyle.SetNormalPadding(FMargin(3.f, 0.f, 3.f, 0.f));
	LibraryButtonStyle.SetPressedPadding(FMargin(3.f, 0.f, 3.f, 0.f));
	Set("LibraryButtonStyle", LibraryButtonStyle);

	FButtonStyle ToolbarButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	ToolbarButtonStyle.SetNormalPadding(FMargin(0.f, 0.f, 0.f, 0.f));
	ToolbarButtonStyle.SetPressedPadding(FMargin(0.f, 0.f, 0.f, 0.f));
	Set("ToolbarButtonStyle", ToolbarButtonStyle);

	Set("MediaButtons", FButtonStyle(FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.PlayControlsButton"))
		.SetNormal(FSlateNoResource())
		.SetDisabled(FSlateNoResource())
		.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.2, 0.2, 0.2, 0.5), 3.f, FVector2D(20.f)))
		.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.1, 0.1, 0.1, 0.5), 3.f, FVector2D(20.f)))
		.SetNormalPadding(FMargin(2.f, 2.f, 2.f, 2.f))
		.SetPressedPadding(FMargin(2.f, 2.f, 2.f, 2.f)));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FMediaViewerStyle::~FMediaViewerStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

FMediaViewerStyle& FMediaViewerStyle::Get()
{
	static FMediaViewerStyle Instance;
	return Instance;
}

} // UE::MediaViewer::Private
