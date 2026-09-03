// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "MetaHumanFaceContourTrackerStyle.h"

class FMetaHumanFaceContourTrackerModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		FMetaHumanFaceContourTrackerStyle::Register();
	}

	virtual void ShutdownModule() override
	{
		FMetaHumanFaceContourTrackerStyle::Unregister();
	}
};

IMPLEMENT_MODULE(FMetaHumanFaceContourTrackerModule, MetaHumanFaceContourTracker)