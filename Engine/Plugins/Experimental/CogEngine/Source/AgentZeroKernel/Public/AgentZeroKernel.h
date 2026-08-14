// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Agent-Zero Kernel Module - Distributed Cognitive Microkernel Framework
 * Provides lightweight distributed cognitive processing across multiple agents
 */
class FAgentZeroKernelModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
