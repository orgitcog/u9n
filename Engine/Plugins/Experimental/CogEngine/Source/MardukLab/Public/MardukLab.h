// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Marduk's-Lab Module - ROS Implementation with Home-Assistant IoT Framework
 * Provides robotics integration and IoT device coordination
 */
class FMardukLabModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
