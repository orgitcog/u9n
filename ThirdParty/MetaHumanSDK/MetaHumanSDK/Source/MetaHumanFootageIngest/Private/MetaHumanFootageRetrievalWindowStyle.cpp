// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanFootageRetrievalWindowStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

FName FMetaHumanFootageRetrievalWindowStyle::StyleName = TEXT("MetaHumanFootageRetrievalWindowStyle");

FMetaHumanFootageRetrievalWindowStyle::FMetaHumanFootageRetrievalWindowStyle()
	: FSlateStyleSet(FMetaHumanFootageRetrievalWindowStyle::StyleName)
{
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Thumb64x64(64.0f, 64.0f);

	SetContentRoot(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetBaseDir() / TEXT("Resources"));

	SetContentRoot(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir());

	Set("CaptureManager.Tabs.CaptureSources", new IMAGE_BRUSH_SVG("Icons/CaptureManagerTabCaptureSources_16", Icon16x16));
	Set("CaptureManager.Tabs.FootageIngest", new IMAGE_BRUSH_SVG("Icons/CaptureManagerTabFootageIngest_16", Icon16x16));
	Set("CaptureManager.Tabs.CaptureManager", new IMAGE_BRUSH_SVG("Icons/CaptureManager_16", Icon16x16));

	Set("CaptureManager.DeviceTypeiPhone", new IMAGE_BRUSH_SVG("Icons/CaptureManagerDeviceTypeiPhone_16", Icon16x16));
	Set("CaptureManager.DeviceTypeiPhoneArchive", new IMAGE_BRUSH_SVG("Icons/CaptureManagerDeviceTypeiPhoneArchive_16", Icon16x16));
	Set("CaptureManager.DeviceTypeiPhoneArchiveRGB", new IMAGE_BRUSH_SVG("Icons/CaptureManagerDeviceTypeiPhoneArchiveRGB_16", Icon16x16));
	Set("CaptureManager.DeviceTypeMonoArchive", new IMAGE_BRUSH_SVG("Icons/CaptureManagerDeviceTypeMonoArchive_16", Icon16x16));
	Set("CaptureManager.DeviceTypeHMC", new IMAGE_BRUSH_SVG("Icons/CaptureManagerDeviceTypeHMC_16", Icon16x16));
	Set("CaptureManager.DeviceTypeUnknown", new IMAGE_BRUSH_SVG("Icons/CaptureManagerDeviceTypeUnknown_16", Icon16x16));

	Set("CaptureManager.DeviceOnline", new IMAGE_BRUSH_SVG("Icons/CaptureSourceStateOnline_16", Icon16x16));
	Set("CaptureManager.DeviceOffline", new IMAGE_BRUSH_SVG("Icons/CaptureSourceStateOffline_16", Icon16x16));

	Set("CaptureManager.Toolbar.AddSource", new IMAGE_BRUSH_SVG("Icons/CaptureManagerToolbarAdd_20", Icon20x20));
	Set("CaptureManager.Toolbar.StartCapture", new IMAGE_BRUSH_SVG("Icons/CaptureManagerToolbarCaptureStart_20", Icon20x20));
	Set("CaptureManager.Toolbar.StopCapture", new IMAGE_BRUSH_SVG("Icons/CaptureManagerToolbarCaptureStop_20", Icon20x20));
	Set("CaptureManager.Toolbar.CaptureSlate", new IMAGE_BRUSH_SVG("Icons/CaptureManagerToolbarCaptureSlate_20", Icon20x20));

	Set("CaptureManager.StartCapture", new IMAGE_BRUSH_SVG("Icons/CaptureManagerCaptureStart_16", Icon16x16));
	Set("CaptureManager.StopCapture", new IMAGE_BRUSH_SVG("Icons/CaptureManagerCaptureStop_16", Icon16x16));
	Set("CaptureManager.CaptureSlate", new IMAGE_BRUSH_SVG("Icons/CaptureManagerToolbarCaptureSlate_16", Icon16x16));

	Set("ClassThumbnail.MetaHumanCaptureSource", new IMAGE_BRUSH_SVG("Icons/AssetCaptureSource_64", Thumb64x64));
	Set("ClassIcon.MetaHumanCaptureSource", new IMAGE_BRUSH_SVG("Icons/AssetCaptureSource_16", Icon16x16));
}

const FName& FMetaHumanFootageRetrievalWindowStyle::GetStyleSetName() const
{
	return StyleName;
}

const FMetaHumanFootageRetrievalWindowStyle& FMetaHumanFootageRetrievalWindowStyle::Get()
{
	static FMetaHumanFootageRetrievalWindowStyle StyleInstance;

	return StyleInstance;
}

void FMetaHumanFootageRetrievalWindowStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

void FMetaHumanFootageRetrievalWindowStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanFootageRetrievalWindowStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}