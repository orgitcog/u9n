// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * CogEngine Module - Main orchestration module for cognitive multi-agent system
 * Integrates all subsystems including OpenCog, Agent-Zero, KoboldAI, Deep-Tree-Echo,
 * Aphrodite-Engine, and Marduk's-Lab
 */
class FCogEngineModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Initialize all cognitive subsystems */
	void InitializeCognitiveSubsystems();
	
	/** Shutdown all cognitive subsystems */
	void ShutdownCognitiveSubsystems();
};
