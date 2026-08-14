// Copyright Deep Tree Echo. All Rights Reserved.

#include "DeepTreeEcho.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FDeepTreeEchoModule"

void FDeepTreeEchoModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("DeepTreeEcho module starting up - Cognitive Architecture initialized"));
}

void FDeepTreeEchoModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("DeepTreeEcho module shutting down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDeepTreeEchoModule, DeepTreeEcho)
