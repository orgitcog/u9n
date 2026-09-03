// Copyright Deep Tree Echo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * DeepTreeEchoAvatar Module
 *
 * Dedicated module for the Deep Tree Echo AI avatar integration layer.
 * Provides:
 * - ADeepTreeEchoAIController: AI controller connecting UDeepTreeEchoCore to Unreal's AI framework
 * - Perception → cognition → action pipeline per tick
 * - Blueprint-exposable AI configuration and runtime state
 *
 * Depends on: DeepTreeEcho (cognitive core), UnrealEcho (avatar/animation layer)
 */
class FDeepTreeEchoAvatarModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the module instance */
	static FDeepTreeEchoAvatarModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FDeepTreeEchoAvatarModule>("DeepTreeEchoAvatar");
	}

	/** Check if module is loaded */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("DeepTreeEchoAvatar");
	}
};
