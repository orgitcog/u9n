// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class IModularGameplayModule : public IModuleInterface
{
public:
	static FORCEINLINE IModularGameplayModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IModularGameplayModule>("ModularGameplay");
	}
};
