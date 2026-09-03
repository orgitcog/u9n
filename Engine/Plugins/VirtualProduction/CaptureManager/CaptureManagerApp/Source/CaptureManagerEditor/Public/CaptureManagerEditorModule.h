// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "LiveLinkHubApplicationMode.h"

class FLiveLinkHubCaptureManagerMode;

class CAPTUREMANAGEREDITOR_API FCaptureManagerEditorModule : public IModuleInterface, public ILiveLinkHubApplicationModeFactory
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	//~ Begin ILiveLinkHubApplicationModeFactory
	virtual TSharedRef<FLiveLinkHubApplicationMode> CreateLiveLinkHubAppMode(TSharedPtr<class FLiveLinkHubApplicationBase> InApp) override;
	//~ End ILiveLinkHubApplicationModeFactory
};
