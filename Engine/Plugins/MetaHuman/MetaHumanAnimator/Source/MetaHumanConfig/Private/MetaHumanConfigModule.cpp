// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "MetaHumanConfigStyle.h"

class FMetaHumanConfigModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		FMetaHumanConfigStyle::Register();
	}

	virtual void ShutdownModule() override
	{
		FMetaHumanConfigStyle::Unregister();
	}
};

IMPLEMENT_MODULE(FMetaHumanConfigModule, MetaHumanConfig)
