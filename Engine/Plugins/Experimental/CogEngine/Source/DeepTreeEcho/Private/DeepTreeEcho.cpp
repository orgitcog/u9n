// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeepTreeEcho.h"

#define LOCTEXT_NAMESPACE "FDeepTreeEchoModule"

void FDeepTreeEchoModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("DeepTreeEcho: Starting Embodied Echo-State Cognition System"));
}

void FDeepTreeEchoModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("DeepTreeEcho: Shutting down Echo-State Network"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDeepTreeEchoModule, DeepTreeEcho)
