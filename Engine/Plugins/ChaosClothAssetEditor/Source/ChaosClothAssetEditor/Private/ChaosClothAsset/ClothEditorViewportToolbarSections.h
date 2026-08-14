// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolMenus.h"

// Common toolbar functions shared by the various cloth editor viewport toolbars.
namespace UE::Chaos::ClothAsset
{
	FToolMenuEntry CreateDynamicLightIntensityItem();
	FToolMenuEntry CreateDynamicSimulationMenuItem();
}
