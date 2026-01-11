// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FMetaHumanPerformanceModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FDelegateHandle MediaTrackEditorBindingHandle;
	FDelegateHandle AudioTrackEditorBindingHandle;

	/** StaticClass is not safe on shutdown, so we cache the name, and use this to unregister on shut down */
	FName ClassToUnregisterOnShutdown;
};