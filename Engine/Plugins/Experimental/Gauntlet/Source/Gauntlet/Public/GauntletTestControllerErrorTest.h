// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GauntletTestController.h"
#include "GauntletTestControllerErrorTest.generated.h"

#define UE_API GAUNTLET_API

UCLASS(MinimalAPI)
class UGauntletTestControllerErrorTest : public UGauntletTestController
{
	GENERATED_BODY()

protected:

	float			ErrorDelay;
	FString			ErrorType;
	bool			RunOnServer;

	UE_API void	OnInit() override;
	UE_API void	OnTick(float TimeDelta)		override;
};

#undef UE_API
