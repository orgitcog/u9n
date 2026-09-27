// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "CoreMinimal.h"

class METAHUMANCALIBRATIONLIB_API FMetaHumanCalibrationLibModule : public IModuleInterface
{
public:

	static FString GetVersion();
};