// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

FMetaHumanPerformanceStyle::FMetaHumanPerformanceStyle()
	: FSlateStyleSet{ TEXT("MetaHumanPerformanceStyle") }
{
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Thumb64x64(64.0f, 64.0f);

	SetContentRoot(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir());

	// Register the icons used by the Performance asset and editors

	//Toolbar versions
	Set("Performance.Toolbar.StartProcessingShot", new IMAGE_BRUSH_SVG("Icons/PerformanceProcessAll_20", Icon20x20));
	Set("Performance.Toolbar.CancelProcessingShot", new IMAGE_BRUSH_SVG("Icons/PerformanceCancelProcessing_20", Icon20x20));
	Set("Performance.Toolbar.ExportAnimation", new IMAGE_BRUSH_SVG("Icons/PerformanceExportAnimation_20", Icon20x20));
	Set("Performance.Toolbar.ExportLevelSequence", new IMAGE_BRUSH_SVG("Icons/PerformanceExportLevelSequence_20", Icon20x20));

	//Menu versions
	Set("Performance.StartProcessingShot", new IMAGE_BRUSH_SVG("Icons/PerformanceProcessAll_16", Icon16x16));
	Set("Performance.CancelProcessingShot", new IMAGE_BRUSH_SVG("Icons/PerformanceCancelProcessing_16", Icon16x16));
	Set("Performance.ExportAnimationProcessingRange", new IMAGE_BRUSH_SVG("Icons/PerformanceExportAnimationProcessingRange_16", Icon16x16));
	Set("Performance.ExportAnimationWholeSequence", new IMAGE_BRUSH_SVG("Icons/PerformanceExportAnimationWholeSequence_16", Icon16x16));
	Set("Performance.ExportAnimation", new IMAGE_BRUSH_SVG("Icons/PerformanceExportAnimation_16", Icon16x16));
	Set("Performance.ExportLevelSequence", new IMAGE_BRUSH_SVG("Icons/PerformanceExportLevelSequence_16", Icon16x16));

	Set("Performance.Tabs.ImageReview", new IMAGE_BRUSH_SVG("Icons/PerformanceTabImageReview_16", Icon16x16));
	Set("Performance.Tabs.ControlRig", new IMAGE_BRUSH_SVG("Icons/PerformanceTabControlRig_16", Icon16x16));

	Set("ClassThumbnail.MetaHumanPerformance", new IMAGE_BRUSH_SVG("Icons/AssetMetaHumanPerformance_64", Thumb64x64));
	Set("ClassIcon.MetaHumanPerformance", new IMAGE_BRUSH_SVG("Icons/AssetMetaHumanPerformance_16", Icon16x16));
}

void FMetaHumanPerformanceStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanPerformanceStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FMetaHumanPerformanceStyle& FMetaHumanPerformanceStyle::Get()
{
	static FMetaHumanPerformanceStyle Inst;
	return Inst;
}

