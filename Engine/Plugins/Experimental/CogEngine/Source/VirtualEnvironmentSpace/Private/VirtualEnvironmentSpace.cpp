// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualEnvironmentSpace.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FVirtualEnvironmentSpaceModule"

void FVirtualEnvironmentSpaceModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("VirtualEnvironmentSpace: Module started"));
}

void FVirtualEnvironmentSpaceModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("VirtualEnvironmentSpace: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVirtualEnvironmentSpaceModule, VirtualEnvironmentSpace)
