// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementSelectionInterface.h"
#include "UObject/Object.h"
#include "ChaosVDSelectionInterface.generated.h"

/**
 * CVD Custom Selection Interface. We don't have any specific implementation overrides yet
 */
UCLASS(MinimalAPI)
class UChaosVDSelectionInterface : public UObject, public ITypedElementSelectionInterface
{
	GENERATED_BODY()
};
