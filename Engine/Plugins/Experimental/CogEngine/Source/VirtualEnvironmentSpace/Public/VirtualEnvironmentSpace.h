// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Virtual Environment Space Module
 * Provides a virtual 3D environment for OpenCog cognitive laboratory
 * and Marduk's Robotics Lab integration, similar to aiangel.io
 */
class FVirtualEnvironmentSpaceModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
