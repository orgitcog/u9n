// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShallowWaterSubsystem.h"
#include "BasicShallowWaterSubsystem.generated.h"

#define UE_API WATERADVANCED_API


UCLASS(MinimalAPI, Blueprintable, Transient)
class UBasicShallowWaterSubsystem : public UShallowWaterSubsystem
{
	GENERATED_BODY()

public:
	UE_API UBasicShallowWaterSubsystem();

	UE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	UE_API virtual bool IsShallowWaterAllowedToInitialize() const override;	
};

#undef UE_API
