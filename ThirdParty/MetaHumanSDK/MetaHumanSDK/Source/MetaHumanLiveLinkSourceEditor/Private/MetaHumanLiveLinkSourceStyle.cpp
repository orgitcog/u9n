// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanLiveLinkSourceStyle.h"

#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"



FMetaHumanLiveLinkSourceStyle::FMetaHumanLiveLinkSourceStyle() : FSlateStyleSet(TEXT("MetaHumanLiveLinkSourceStyle"))
{
	SetContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	Set("Refresh", new IMAGE_BRUSH_SVG("Starship/Common/Update", CoreStyleConstants::Icon16x16));
}

void FMetaHumanLiveLinkSourceStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanLiveLinkSourceStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FMetaHumanLiveLinkSourceStyle& FMetaHumanLiveLinkSourceStyle::Get()
{
	static FMetaHumanLiveLinkSourceStyle Inst;
	return Inst;
}