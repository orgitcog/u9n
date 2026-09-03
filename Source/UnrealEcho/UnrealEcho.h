// Copyright Deep Tree Echo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * UnrealEcho Module
 *
 * Unreal Engine integration module providing:
 * - Avatar visualization and animation
 * - Live2D Cubism integration for 2D avatars
 * - 3D avatar systems with cognitive state visualization
 * - Neurochemical simulation
 * - Personality trait systems
 * - Environmental interaction
 */
class FUnrealEchoModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the module instance */
	static FUnrealEchoModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUnrealEchoModule>("UnrealEcho");
	}

	/** Check if module is loaded */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UnrealEcho");
	}
};
