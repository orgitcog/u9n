// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Live2D Cubism Core Module - Integration with Live2D Cubism SDK
 * Provides 2D character animation and rendering using Live2D technology
 */
class FLive2DCubismCoreModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** Check if Live2D SDK is available */
	bool IsSDKAvailable() const;
};
