// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FSlateStyleSet;

class FCaptureManagerEditorSettingsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	void PostEngineInit();
	void EnginePreExit();

	TSharedPtr<FSlateStyleSet> StyleSet;
};
