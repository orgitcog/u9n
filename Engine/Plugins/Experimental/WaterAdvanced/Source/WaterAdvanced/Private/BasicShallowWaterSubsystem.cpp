// Copyright Epic Games, Inc. All Rights Reserved.

#include "BasicShallowWaterSubsystem.h"

UBasicShallowWaterSubsystem::UBasicShallowWaterSubsystem()
{
}

bool UBasicShallowWaterSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TObjectPtr<UShallowWaterSettings> TmpSettings = GetMutableDefault<UShallowWaterSettings>();
	if (TmpSettings && !TmpSettings->UseDefaultShallowWaterSubsystem)
	{
		return false;
	}
	else
	{
		return Super::ShouldCreateSubsystem(Outer);
	}	
}

bool UBasicShallowWaterSubsystem::IsShallowWaterAllowedToInitialize() const
{
	TObjectPtr<UShallowWaterSettings> TmpSettings = GetMutableDefault<UShallowWaterSettings>();
	if (TmpSettings)
	{
		return TmpSettings->UseDefaultShallowWaterSubsystem;
	}

	return false;
}