// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * The public interface of the MusicEnvironment module
 */
class FMusicEnvironmentModule
	: public IModuleInterface
{
public:

	static inline FMusicEnvironmentModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FMusicEnvironmentModule>("MusicEnvironment");
	}
};
