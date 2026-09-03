// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonMenuExtensionsModule.h"
#include "BufferVisualizationMenuCommands.h"
#include "NaniteVisualizationMenuCommands.h"
#include "LumenVisualizationMenuCommands.h"
#include "SubstrateVisualizationMenuCommands.h"
#include "GroomVisualizationMenuCommands.h"
#include "VirtualShadowMapVisualizationMenuCommands.h"
#include "VirtualTextureVisualizationMenuCommands.h"
#include "ShowFlagMenuCommands.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FCommonMenuExtensionsModule, CommonMenuExtensions);

void FCommonMenuExtensionsModule::StartupModule()
{
	FBufferVisualizationMenuCommands::Register();
	FNaniteVisualizationMenuCommands::Register();
	FLumenVisualizationMenuCommands::Register();
	FSubstrateVisualizationMenuCommands::Register();
	FGroomVisualizationMenuCommands::Register();
	FVirtualShadowMapVisualizationMenuCommands::Register();
	FVirtualTextureVisualizationMenuCommands::Register();
	FShowFlagMenuCommands::Register();
}

void FCommonMenuExtensionsModule::ShutdownModule()
{
	FShowFlagMenuCommands::Unregister();
	FVirtualTextureVisualizationMenuCommands::Unregister();
	FVirtualShadowMapVisualizationMenuCommands::Unregister();
	FNaniteVisualizationMenuCommands::Unregister();
	FGroomVisualizationMenuCommands::Unregister();
	FSubstrateVisualizationMenuCommands::Unregister();
	FLumenVisualizationMenuCommands::Unregister();
	FBufferVisualizationMenuCommands::Unregister();
}