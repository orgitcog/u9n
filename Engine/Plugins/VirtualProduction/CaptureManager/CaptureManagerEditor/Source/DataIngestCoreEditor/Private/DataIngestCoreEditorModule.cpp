// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FDataIngestCoreEditorModule : public IModuleInterface
{
public:
	
	void StartupModule()
	{
	}

	void ShutdownModule()
	{
	}
};

IMPLEMENT_MODULE(FDataIngestCoreEditorModule, DataIngestCoreEditor)
