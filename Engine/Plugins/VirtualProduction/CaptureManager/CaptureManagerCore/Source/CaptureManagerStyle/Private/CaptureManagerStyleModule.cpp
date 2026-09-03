// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureManagerStyleModule.h"

#include "Styling/SlateStyleRegistry.h"
#include "CaptureManagerStyle.h"


IMPLEMENT_MODULE(FCaptureManagerStyleModule, CaptureManagerStyle)

void FCaptureManagerStyleModule::StartupModule()
{
	FSlateStyleRegistry::RegisterSlateStyle(FCaptureManagerStyle::Get());
	FCaptureManagerStyle::ReloadTextures();
}

void FCaptureManagerStyleModule::ShutdownModule()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(FCaptureManagerStyle::Get());
}
