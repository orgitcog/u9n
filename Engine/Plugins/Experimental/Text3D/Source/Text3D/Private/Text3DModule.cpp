// Copyright Epic Games, Inc. All Rights Reserved.

#include "Text3DModule.h"

#include "Logs/Text3DLogs.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogText3D);

IMPLEMENT_MODULE(FText3DModule, Text3D);

FText3DModule::FText3DModule()
{
#if WITH_FREETYPE
	FreeTypeLib = nullptr;
#endif
}

void FText3DModule::StartupModule()
{
#if WITH_FREETYPE
	FT_Init_FreeType(&FreeTypeLib);
#endif
}

void FText3DModule::ShutdownModule()
{
#if WITH_FREETYPE
	FT_Done_FreeType(FreeTypeLib);
	FreeTypeLib = nullptr;
#endif
}

#if WITH_FREETYPE
FT_Library FText3DModule::GetFreeTypeLibrary()
{
	const FText3DModule& Instance = FModuleManager::LoadModuleChecked<FText3DModule>("Text3D");
	return Instance.FreeTypeLib;
}
#endif