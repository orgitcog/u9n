// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "UIFWidgetOwner.generated.h"

class UGameInstance;
class APlayerController;
class UWorld;

/**
 *
 */
USTRUCT()
struct FUIFrameworkWidgetOwner
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APlayerController> PlayerController = nullptr;

	UPROPERTY()
	TObjectPtr<UGameInstance> GameInstance = nullptr;

	UPROPERTY()
	TObjectPtr<UWorld> World = nullptr;
};
