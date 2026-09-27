// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FLiveLinkFaceSourceModule : public IModuleInterface
{
public:
	
	// ~IModuleInterface Interface
	
	virtual void ShutdownModule() override;
	virtual void StartupModule() override;
	
	// ~IModuleInterface Interface
};
