// Copyright Deep Tree Echo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Deep Tree Echo Module
 *
 * Core cognitive architecture module providing:
 * - 12-step cognitive processing cycle
 * - Echo State Networks (Reservoir computing)
 * - 4E Embodied Cognition (Embodied, Embedded, Enacted, Extended)
 * - Recursive Mutual Awareness (3 concurrent consciousness streams)
 * - Hierarchical goal management
 */
class FDeepTreeEchoModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the module instance */
	static FDeepTreeEchoModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FDeepTreeEchoModule>("DeepTreeEcho");
	}

	/** Check if module is loaded */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("DeepTreeEcho");
	}
};
