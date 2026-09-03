// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * OpenCog Core Module - Autonomous Multi-Agent Orchestration Workbench
 * Provides distributed agent coordination, goal management, and cognitive processing
 */
class FOpenCogCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
