// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextConfig.generated.h"

#define UE_API ANIMNEXT_API

UCLASS(MinimalAPI, Config=AnimNext)
class UAnimNextConfig : public UObject
{
	GENERATED_BODY()

private:
	// UObject interface
#if WITH_EDITOR
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

#undef UE_API
