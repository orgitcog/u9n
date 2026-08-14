// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FMetaHumanBatchMenuExtensions;

class FMetaHumanBatchProcessorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TUniquePtr<FMetaHumanBatchMenuExtensions> MetaHumanBatchMenuExtensions;
};
