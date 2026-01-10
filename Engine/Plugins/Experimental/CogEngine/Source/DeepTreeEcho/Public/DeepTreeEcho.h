// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright Deep Tree Echo. All Rights Reserved.
//
// MERGED VERSION: Contains functionality from both Engine/Plugins and Source versions
// Canonical location: Source/DeepTreeEcho/DeepTreeEcho.h (primary)

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Deep-Tree-Echo Module - Embodied Echo-State Network Cognition
 * 
 * Core cognitive architecture module providing:
 * - 12-step cognitive processing cycle
 * - Echo State Networks (Reservoir computing)
 * - 4E Embodied Cognition (Embodied, Embedded, Enacted, Extended)
 * - Recursive Mutual Awareness (3 concurrent consciousness streams)
 * - Hierarchical goal management
 * - Hierarchical echo-state networks for deep cognitive processing
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
