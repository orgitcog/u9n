// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenCogCore.h"

#define LOCTEXT_NAMESPACE "FOpenCogCoreModule"

void FOpenCogCoreModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("OpenCogCore: Starting Autonomous Multi-Agent Orchestration Workbench"));
}

void FOpenCogCoreModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("OpenCogCore: Shutting down Multi-Agent Orchestration"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOpenCogCoreModule, OpenCogCore)
