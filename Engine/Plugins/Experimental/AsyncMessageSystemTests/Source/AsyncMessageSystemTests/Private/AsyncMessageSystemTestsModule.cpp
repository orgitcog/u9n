// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncMessageSystemTestsModule.h"
#include "Modules/ModuleManager.h"

bool FAsyncMessageSystemTestsModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("AsyncMessageSystemTests");
}

IMPLEMENT_MODULE(FAsyncMessageSystemTestsModule, AsyncMessageSystemTests)