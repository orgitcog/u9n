// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "MetaHumanCharacterLog.h"
#include "MetaHumanCharacterStyle.h"

DEFINE_LOG_CATEGORY(LogMetaHumanCharacter);

class FMetaHumanCharacterModule
	: public IModuleInterface
{
public:
	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		FMetaHumanCharacterStyle::Register();
	}

	virtual void ShutdownModule() override
	{
		FMetaHumanCharacterStyle::Unregister();
	}
};

IMPLEMENT_MODULE(FMetaHumanCharacterModule, MetaHumanCharacter);
