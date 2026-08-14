// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "MetaHumanToolkitStyle.h"
#include "MetaHumanToolkitCommands.h"

class FMetaHumanToolkitModule
	: public IModuleInterface
{
public:

	//~Begin IModuleInterface interface
	virtual void StartupModule() override
	{
		FMetaHumanToolkitCommands::Register();
		FMetaHumanToolkitStyle::Register();
	}

	virtual void ShutdownModule() override
	{
		FMetaHumanToolkitStyle::Unregister();
		FMetaHumanToolkitCommands::Unregister();
	}
	//~End IModuleInterface interface
};

IMPLEMENT_MODULE(FMetaHumanToolkitModule, MetaHumanToolkit)