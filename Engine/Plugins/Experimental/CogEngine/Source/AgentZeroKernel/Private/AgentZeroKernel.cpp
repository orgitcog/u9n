// Copyright Epic Games, Inc. All Rights Reserved.

#include "AgentZeroKernel.h"

#define LOCTEXT_NAMESPACE "FAgentZeroKernelModule"

void FAgentZeroKernelModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("AgentZeroKernel: Starting Distributed Cognitive Microkernel Framework"));
}

void FAgentZeroKernelModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("AgentZeroKernel: Shutting down Microkernel Framework"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAgentZeroKernelModule, AgentZeroKernel)
