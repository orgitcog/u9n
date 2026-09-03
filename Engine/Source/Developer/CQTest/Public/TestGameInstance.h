// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"

#include "TestGameInstance.generated.h"

#define UE_API CQTEST_API

UCLASS(MinimalAPI, config=Game, transient, BlueprintType, Blueprintable)
class UTestGameInstance : public UGameInstance
{
	GENERATED_UCLASS_BODY()

public:
	UE_API void InitializeForTest(UWorld& InWorld);
};

#undef UE_API
