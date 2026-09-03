// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanFaceAnimationSolverStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

FName FMetaHumanFaceAnimationSolverStyle::StyleName = TEXT("MetaHumanFaceAnimationSolverStyle");

FMetaHumanFaceAnimationSolverStyle::FMetaHumanFaceAnimationSolverStyle()
	: FSlateStyleSet(FMetaHumanFaceAnimationSolverStyle::StyleName)
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Thumb64x64(64.0, 64.0f);

	SetContentRoot(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir());

	Set("ClassThumbnail.MetaHumanFaceAnimationSolver", new IMAGE_BRUSH_SVG("Icons/AssetFaceAnimationSolver_64", Thumb64x64));
	Set("ClassIcon.MetaHumanFaceAnimationSolver", new IMAGE_BRUSH_SVG("Icons/AssetFaceAnimationSolver_16", Icon16x16));
}

const FName& FMetaHumanFaceAnimationSolverStyle::GetStyleSetName() const
{
	return StyleName;
}

const FMetaHumanFaceAnimationSolverStyle& FMetaHumanFaceAnimationSolverStyle::Get()
{
	static FMetaHumanFaceAnimationSolverStyle StyleInstance;

	return StyleInstance;
}

void FMetaHumanFaceAnimationSolverStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

void FMetaHumanFaceAnimationSolverStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanFaceAnimationSolverStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}
