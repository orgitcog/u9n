// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#include "Delegates/IDelegateInstance.h"

class FPSDImporterMaterialDesignerBridgeModule
	: public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

protected:
	FDelegateHandle TextureResetDelegate;
};
