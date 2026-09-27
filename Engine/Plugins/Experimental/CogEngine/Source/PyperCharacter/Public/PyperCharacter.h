// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Pyper Character Module - Deep Tree Echo Pyper Integration
 * Implements Pyper mascot character with Echo State Network cognitive architecture
 */
class FPyperCharacterModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
