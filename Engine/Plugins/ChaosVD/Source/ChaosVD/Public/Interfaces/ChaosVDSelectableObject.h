// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "ChaosVDSelectableObject.generated.h"

UINTERFACE(MinimalAPI)
class UChaosVDSelectableObject : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface used for any object that can be selected in CVD and need to process selection events performed on it.
 */
class IChaosVDSelectableObject
{
	GENERATED_BODY()

public:
	CHAOSVD_API virtual void HandleSelected() PURE_VIRTUAL(IChaosVDSelectionAware::HandleSelected);
	CHAOSVD_API virtual void HandleDeSelected() PURE_VIRTUAL(IChaosVDSelectionAware::HandleUnSelected);
};
