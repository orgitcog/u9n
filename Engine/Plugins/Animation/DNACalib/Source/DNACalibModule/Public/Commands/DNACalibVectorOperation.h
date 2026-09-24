// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/ObjectMacros.h"

#include "DNACalibVectorOperation.generated.h"

UENUM(BlueprintType)
enum class EDNACalibVectorOperation: uint8
{
	Interpolate,
	Add,
	Subtract,
	Multiply
};