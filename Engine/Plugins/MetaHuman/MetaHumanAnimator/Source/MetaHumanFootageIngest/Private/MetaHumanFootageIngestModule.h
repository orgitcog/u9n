// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FMetaHumanFootageIngestModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** This function will bring up the footage retrieval window */
	void CaptureManagerMenuSelected();

private:

	class FCaptureManager* CaptureManager = nullptr;
};