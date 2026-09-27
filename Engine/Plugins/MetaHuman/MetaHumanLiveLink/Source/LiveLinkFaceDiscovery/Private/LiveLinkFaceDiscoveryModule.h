// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FLiveLinkFaceDiscoveryModule : public IModuleInterface
{
public:

	// ~IModuleInterface Interface
	
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

	// ~IModuleInterface Interface
};
