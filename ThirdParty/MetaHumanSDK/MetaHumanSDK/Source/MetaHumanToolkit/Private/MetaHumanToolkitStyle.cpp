// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanToolkitStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

FMetaHumanToolkitStyle::FMetaHumanToolkitStyle()
	: FSlateStyleSet{ TEXT("MetaHumanToolkitStyle") }
{
	const FVector2D Icon16x16(16.0f, 16.0f);

	SetContentRoot(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir());

	Set("MetaHuman Toolkit.Viewport.CameraOptions", new IMAGE_BRUSH_SVG("Icons/ToolkitCameraOptions_16", Icon16x16));
	
	Set("MetaHuman Toolkit.Viewport.ABMode.Single", new IMAGE_BRUSH_SVG("Icons/ToolkitABModeSingle_16", Icon16x16));
	Set("MetaHuman Toolkit.Viewport.ABMode.Wipe", new IMAGE_BRUSH_SVG("Icons/ToolkitABModeWipe_16", Icon16x16));
	Set("MetaHuman Toolkit.Viewport.ABMode.Dual", new IMAGE_BRUSH_SVG("Icons/ToolkitABModeDual_16", Icon16x16));

	Set("MetaHuman Toolkit.ABSplit.A.Small", new IMAGE_BRUSH_SVG("Icons/ToolkitABSplit_A_Small_16", Icon16x16));
	Set("MetaHuman Toolkit.ABSplit.A.Large", new IMAGE_BRUSH_SVG("Icons/ToolkitABSplit_A_Large_16", Icon16x16));
	Set("MetaHuman Toolkit.ABSplit.B.Small", new IMAGE_BRUSH_SVG("Icons/ToolkitABSplit_B_Small_16", Icon16x16));
	Set("MetaHuman Toolkit.ABSplit.B.Large", new IMAGE_BRUSH_SVG("Icons/ToolkitABSplit_B_Large_16", Icon16x16));

	Set("MetaHuman Toolkit.Tabs.Timeline", new IMAGE_BRUSH_SVG("Icons/ToolkitTabsTimeline_16", Icon16x16));
	Set("MetaHuman Toolkit.Tabs.Markers", new IMAGE_BRUSH_SVG("Icons/ToolkitTabsMarkers_16", Icon16x16));
	Set("MetaHuman Toolkit.Tabs.ABViewport", new IMAGE_BRUSH_SVG("Icons/ToolkitTabsABViewport_16", Icon16x16));
}

void FMetaHumanToolkitStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanToolkitStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FMetaHumanToolkitStyle& FMetaHumanToolkitStyle::Get()
{
	static FMetaHumanToolkitStyle Inst;
	return Inst;
}