// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"



class FMetaHumanCaptureDataEditorModule
	: public IModuleInterface
{

public:

	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** StaticClass is not safe on shutdown, so we cache the name, and use this to unregister on shut down */
	FName ClassToUnregisterOnShutdown;
};
