// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanFaceContourTrackerStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

FName FMetaHumanFaceContourTrackerStyle::StyleName = TEXT("MetaHumanFaceContourTrackerStyle");

FMetaHumanFaceContourTrackerStyle::FMetaHumanFaceContourTrackerStyle()
	: FSlateStyleSet(FMetaHumanFaceContourTrackerStyle::StyleName)
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Thumb64x64(64.0, 64.0f);

	SetContentRoot(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir());

	Set("ClassThumbnail.MetaHumanFaceContourTrackerAsset", new IMAGE_BRUSH_SVG("Icons/AssetFaceContourTracker_64", Thumb64x64));
	Set("ClassIcon.MetaHumanFaceContourTrackerAsset", new IMAGE_BRUSH_SVG("Icons/AssetFaceContourTracker_16", Icon16x16));
}

const FName& FMetaHumanFaceContourTrackerStyle::GetStyleSetName() const
{
	return StyleName;
}

const FMetaHumanFaceContourTrackerStyle& FMetaHumanFaceContourTrackerStyle::Get()
{
	static FMetaHumanFaceContourTrackerStyle StyleInstance;

	return StyleInstance;
}

void FMetaHumanFaceContourTrackerStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

void FMetaHumanFaceContourTrackerStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanFaceContourTrackerStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}