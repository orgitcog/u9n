// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncMessageSystemModule.h"
#include "Modules/ModuleManager.h"

bool FAsyncMessageSystemModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("AsyncMessageSystem");
}

IMPLEMENT_MODULE(FAsyncMessageSystemModule, AsyncMessageSystem)
