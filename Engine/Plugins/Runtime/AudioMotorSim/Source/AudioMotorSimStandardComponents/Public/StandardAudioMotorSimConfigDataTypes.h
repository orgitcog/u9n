// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMotorSimConfigData.h"

#include "StandardAudioMotorSimConfigDataTypes.generated.h"


USTRUCT(BlueprintType)
struct FMotorPhysicsSimConfigData : public FAudioMotorSimConfigData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Force")
	float Weight = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Force")
	float EngineTorque = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Force")
	float BrakingHorsePower = 6000.f;
};
