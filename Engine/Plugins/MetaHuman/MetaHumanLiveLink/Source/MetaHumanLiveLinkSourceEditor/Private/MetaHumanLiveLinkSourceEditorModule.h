// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FMetaHumanLiveLinkSourceEditorModule : public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	TArray<FName> PropertiesToUnregisterOnShutdown;
	TArray<FName> ClassesToUnregisterOnShutdown;
};