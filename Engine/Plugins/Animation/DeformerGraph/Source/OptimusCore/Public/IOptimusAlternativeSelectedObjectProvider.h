// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusAlternativeSelectedObjectProvider.generated.h"


UINTERFACE(MinimalAPI)
class UOptimusAlternativeSelectedObjectProvider :
	public UInterface
{
	GENERATED_BODY()
};


class IOptimusAlternativeSelectedObjectProvider
{
	GENERATED_BODY()

public:
	virtual UObject* GetObjectToShowWhenSelected() const = 0;
};
