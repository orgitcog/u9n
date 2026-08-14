// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "MetaHumanFaceAnimationSolverStyle.h"

class FMetaHumanFaceAnimationSolverModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		FMetaHumanFaceAnimationSolverStyle::Register();
	}

	virtual void ShutdownModule() override
	{
		FMetaHumanFaceAnimationSolverStyle::Unregister();
	}
};

IMPLEMENT_MODULE(FMetaHumanFaceAnimationSolverModule, MetaHumanFaceAnimationSolver)
