// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateIM.h"

#include "Misc/SlateIMLogging.h"
#include "Misc/SlateIMManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogSlateIM);

void FSlateIMModule::StartupModule()
{
	SlateIM::FSlateIMManager::Initialize();
}

void FSlateIMModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FSlateIMModule, SlateIM)
