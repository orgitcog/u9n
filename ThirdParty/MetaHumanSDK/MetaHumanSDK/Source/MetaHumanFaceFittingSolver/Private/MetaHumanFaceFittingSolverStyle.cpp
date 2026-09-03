// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanFaceFittingSolverStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

FName FMetaHumanFaceFittingSolverStyle::StyleName = TEXT("MetaHumanFaceFittingSolverStyle");

FMetaHumanFaceFittingSolverStyle::FMetaHumanFaceFittingSolverStyle()
	: FSlateStyleSet(FMetaHumanFaceFittingSolverStyle::StyleName)
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Thumb64x64(64.0, 64.0f);

	SetContentRoot(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir());

	Set("ClassThumbnail.MetaHumanFaceFittingSolver", new IMAGE_BRUSH_SVG("Icons/AssetFaceFittingSolver_64", Thumb64x64));
	Set("ClassIcon.MetaHumanFaceFittingSolver", new IMAGE_BRUSH_SVG("Icons/AssetFaceFittingSolver_16", Icon16x16));
}

const FName& FMetaHumanFaceFittingSolverStyle::GetStyleSetName() const
{
	return StyleName;
}

const FMetaHumanFaceFittingSolverStyle& FMetaHumanFaceFittingSolverStyle::Get()
{
	static FMetaHumanFaceFittingSolverStyle StyleInstance;

	return StyleInstance;
}

void FMetaHumanFaceFittingSolverStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

void FMetaHumanFaceFittingSolverStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanFaceFittingSolverStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}
