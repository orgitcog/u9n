// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/** Hair dataflow construction module public interface */
class FHairStrandsDataflowModule : public IModuleInterface
{
public :
	
	//~ IModuleInterface begin interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ IModuleInterface end interface
};
