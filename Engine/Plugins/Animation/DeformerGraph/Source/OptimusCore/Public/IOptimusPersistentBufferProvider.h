// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusPersistentBufferProvider.generated.h"


class FOptimusPersistentBufferPool;

UINTERFACE(MinimalAPI)
class UOptimusPersistentBufferProvider :
	public UInterface
{
	GENERATED_BODY()
};


class IOptimusPersistentBufferProvider
{
	GENERATED_BODY()

public:
	virtual void SetBufferPool(TSharedPtr<FOptimusPersistentBufferPool> InBufferPool) = 0;
};
