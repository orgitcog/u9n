// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

#include "MediaRWManager.h"

class CAPTUREMANAGERMEDIARW_API FCaptureManagerMediaRWModule : public IModuleInterface
{
public:

	void StartupModule();
	void ShutdownModule();

	FMediaRWManager& Get();

private:

	TUniquePtr<FMediaRWManager> MediaRWManager;
};