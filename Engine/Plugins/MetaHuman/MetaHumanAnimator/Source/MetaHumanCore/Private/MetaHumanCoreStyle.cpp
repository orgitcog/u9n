// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

FName FMetaHumanCoreStyle::StyleName = TEXT("MetaHumanCoreStyle");

FMetaHumanCoreStyle::FMetaHumanCoreStyle()
	: FSlateStyleSet(FMetaHumanCoreStyle::StyleName)
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Thumb64x64(64, 64);

	SetContentRoot(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir());

	Set("ClassThumbnail.MetaHumanCameraCalibration", new IMAGE_BRUSH_SVG("Icons/AssetCameraCalibration_64", Thumb64x64));
	Set("ClassIcon.MetaHumanCameraCalibration", new IMAGE_BRUSH_SVG("Icons/AssetCameraCalibration_16", Icon16x16));
}

const FName& FMetaHumanCoreStyle::GetStyleSetName() const
{
	return StyleName;
}

const FMetaHumanCoreStyle& FMetaHumanCoreStyle::Get()
{
	static FMetaHumanCoreStyle StyleInstance;

	return StyleInstance;
}

void FMetaHumanCoreStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

void FMetaHumanCoreStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanCoreStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}