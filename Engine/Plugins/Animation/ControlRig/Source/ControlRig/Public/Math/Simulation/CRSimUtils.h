// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define UE_API CONTROLRIG_API

class FCRSimUtils
{
public:

	static UE_API void ComputeWeightsFromMass(float MassA, float MassB, float& OutWeightA, float& OutWeightB);
};

#undef UE_API
