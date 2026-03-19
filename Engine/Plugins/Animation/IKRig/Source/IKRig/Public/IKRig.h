// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// a UProperty metadata tag used to indicate that a property should trigger reinitialization when edited
static const TCHAR* IKRigReinitOnEditMetaLabel = TEXT("ReinitializeOnEdit");

class FIKRigModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
