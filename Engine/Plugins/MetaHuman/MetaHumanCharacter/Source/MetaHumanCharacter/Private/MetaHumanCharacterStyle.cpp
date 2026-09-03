// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"
#include "Interfaces/IPluginManager.h"

FMetaHumanCharacterStyle::FMetaHumanCharacterStyle()
	: FSlateStyleSet{ TEXT("MetaHumanCharacterStyle") }
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);
	check(Plugin.IsValid());

	SetContentRoot(Plugin->GetContentDir());

	const FVector2D Icon16{ 16.0, 16.0 };
	const FVector2D Icon64{ 64.0, 64.0 };

	Set(TEXT("ClassThumbnail.MetaHumanCharacter"), new IMAGE_BRUSH_SVG("UI/Icons/Asset_Character", Icon64));
	Set(TEXT("ClassIcon.MetaHumanCharacter"), new IMAGE_BRUSH_SVG("UI/Icons/Asset_Character", Icon16));
}

void FMetaHumanCharacterStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanCharacterStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

const FMetaHumanCharacterStyle& FMetaHumanCharacterStyle::Get()
{
	static FMetaHumanCharacterStyle Inst;
	return Inst;
}