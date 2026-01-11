// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "MetaHumanFaceFittingSolverStyle.h"

class FMetaHumanFaceFittingSolverModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		FMetaHumanFaceFittingSolverStyle::Register();
	}

	virtual void ShutdownModule() override
	{
		FMetaHumanFaceFittingSolverStyle::Unregister();
	}
};

IMPLEMENT_MODULE(FMetaHumanFaceFittingSolverModule, MetaHumanFaceFittingSolver)
