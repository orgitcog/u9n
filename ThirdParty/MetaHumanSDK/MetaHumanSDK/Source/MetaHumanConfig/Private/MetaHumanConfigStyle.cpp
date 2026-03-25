// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanConfigStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

FName FMetaHumanConfigStyle::StyleName = TEXT("MetaHumanConfigStyle");

FMetaHumanConfigStyle::FMetaHumanConfigStyle()
	: FSlateStyleSet(FMetaHumanConfigStyle::StyleName)
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Thumb64x64(64.0, 64.0f);

	SetContentRoot(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir());

	Set("ClassThumbnail.MetaHumanConfig", new IMAGE_BRUSH_SVG("Icons/AssetMetaHumanConfig_64", Thumb64x64));
	Set("ClassIcon.MetaHumanConfig", new IMAGE_BRUSH_SVG("Icons/AssetMetaHumanConfig_16", Icon16x16));
}

const FName& FMetaHumanConfigStyle::GetStyleSetName() const
{
	return StyleName;
}

const FMetaHumanConfigStyle& FMetaHumanConfigStyle::Get()
{
	static FMetaHumanConfigStyle StyleInstance;

	return StyleInstance;
}

void FMetaHumanConfigStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

void FMetaHumanConfigStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanConfigStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}
