// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Neuro Fusion Module - Pyper vs Neuro Superhot Battle System
 * Implements competitive and fusion mechanics between cognitive entities
 */
class FNeuroFusionModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
