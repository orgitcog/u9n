// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "StaticCameraTests/AutomatedStaticCameraPerfTestBase.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "AutomatedPlacedStaticCameraPerfTest.generated.h"

#define UE_API AUTOMATEDPERFTESTING_API

UCLASS(MinimalAPI)
class UAutomatedPlacedStaticCameraPerfTest : public UAutomatedStaticCameraPerfTestBase
{
	GENERATED_BODY()

public:
	UE_API virtual TArray<ACameraActor*> GetMapCameraActors() override;
};

#undef UE_API
