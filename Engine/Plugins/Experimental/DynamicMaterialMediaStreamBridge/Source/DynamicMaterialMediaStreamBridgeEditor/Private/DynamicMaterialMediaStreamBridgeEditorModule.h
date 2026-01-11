// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

/**
 * Media Stream Material Designer Bridge Editor - Integrates the Media Stream plugin with the Material Designer.
 */
class FDynamicMaterialMediaStreamBridgeEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	//~ End IModuleInterface
};
