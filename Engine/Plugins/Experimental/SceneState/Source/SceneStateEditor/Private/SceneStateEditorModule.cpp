// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateEditorModule.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "SceneStateEditorLog.h"
#include "Tasks/SceneStateTaskDescRegistry.h"

DEFINE_LOG_CATEGORY(LogSceneStateEditor);

IMPLEMENT_MODULE(FSceneStateEditorModule, SceneStateEditor)

void FSceneStateEditorModule::StartupModule()
{
	OnPostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FSceneStateEditorModule::OnPostEngineInit);
}

void FSceneStateEditorModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.Remove(OnPostEngineInitHandle);
	OnPostEngineInitHandle.Reset();
}

void FSceneStateEditorModule::OnPostEngineInit()
{
	FSceneStateTaskDescRegistry::GlobalRegistry.CacheTaskDescs();
}
