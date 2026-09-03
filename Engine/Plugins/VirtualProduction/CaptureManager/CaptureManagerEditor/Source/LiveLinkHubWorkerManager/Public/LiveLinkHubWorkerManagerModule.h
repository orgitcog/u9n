// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

#include "LiveLinkHubWorkerManager.h"

class LIVELINKHUBWORKERMANAGER_API FLiveLinkHubWorkerManagerModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedRef<FLiveLinkHubWorkerManager> GetManager();

private:

	void PostEngineInit();
	void EnginePreExit();

	bool CheckExportServerAvailability(float InDelay);

	TSharedPtr<FLiveLinkHubWorkerManager> Manager;
	FTSTicker::FDelegateHandle Delegate;
};
