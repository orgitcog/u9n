// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEditorCoreStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

FAvaEditorCoreStyle::FAvaEditorCoreStyle()
	: FSlateStyleSet(TEXT("AvaEditorCore"))
{
	const FVector2f Icon16(16.f);

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);
	check(Plugin.IsValid());

	FSlateStyleSet::SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	FSlateStyleSet::SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));

	Set("Icons.MotionDesign", new IMAGE_BRUSH(TEXT("Icons/ToolboxIcons/toolbox"), Icon16));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FAvaEditorCoreStyle::~FAvaEditorCoreStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
