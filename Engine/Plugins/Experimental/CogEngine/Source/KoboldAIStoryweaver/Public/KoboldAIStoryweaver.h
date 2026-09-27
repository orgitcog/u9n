// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * KoboldAI Storyweaver Module - Autognostic World-Building and Character Arc System
 * Maintains narrative coherence for billion-player-specific character arcs
 */
class FKoboldAIStoryweaverModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
