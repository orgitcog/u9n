// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

#include "MetaHumanPipelineModule.generated.h"

class FMetaHumanPipelineModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

UENUM()
enum class EMetaHumanPipelineModuleTemp : uint8 // Temp measure to not break builds!
{
	None = 0,
};
