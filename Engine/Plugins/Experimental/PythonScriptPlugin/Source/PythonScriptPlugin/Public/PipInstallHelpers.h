// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPipInstall.h"

// PipInstall dialog responses
enum EPipInstallDialogResult
{
	BackgroundInstall,
	Finished,
	Canceled,
	Error,
};

/**
 * Simplified UI helper for creating a modal dialog and running (background) installs
 */
class PYTHONSCRIPTPLUGIN_API FPipInstallHelper
{
public:
	// Check if pip install is required
	static int32 GetNumPackagesToInstall();
	
	// Show notification that pip install is required
	static EPipInstallDialogResult ShowPipInstallDialog(bool bAllowBackgroundInstall = true);

	// Run a headless pip install (for commandlets/build machines)
	static bool LaunchHeadlessPipInstall();
};
