// Copyright Deep Tree Echo. All Rights Reserved.

#include "UnrealEcho.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FUnrealEchoModule"

void FUnrealEchoModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("UnrealEcho module starting up - Avatar systems initialized"));
}

void FUnrealEchoModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("UnrealEcho module shutting down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealEchoModule, UnrealEcho)
