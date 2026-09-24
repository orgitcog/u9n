// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class UAnimatorKitSettings;

class FAnimatorKitSettingsModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void UpdateSettings(const UAnimatorKitSettings* InSettings);
};
