// Copyright Epic Games, Inc. All Rights Reserved.

#include "IMassNavMeshNavigationModule.h"

class FMassNavMeshNavigationModule : public IMassNavMeshNavigationModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FMassNavMeshNavigationModule, MassNavMeshNavigation)


void FMassNavMeshNavigationModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void FMassNavMeshNavigationModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}
