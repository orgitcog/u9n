// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class ITakeRecorderNamingTokensModule : public IModuleInterface
{
public:
	/** The Take Recorder namespace used for processing naming tokens. */
	static FString GetTakeRecorderNamespace()
	{
		return TEXT("tr");
	}
};
