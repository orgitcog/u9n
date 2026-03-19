// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

#include "Async/CaptureTimerManager.h"

class CAPTUREUTILS_API FCaptureUtilsModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedRef<UE::CaptureManager::FCaptureTimerManager> GetTimerManager();

private:

	TSharedPtr<UE::CaptureManager::FCaptureTimerManager> TimerManager;
};