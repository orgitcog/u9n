// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "ChaosVDPooledObject.generated.h"

UINTERFACE(MinimalAPI)
class UChaosVDPooledObject : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface used by any object that can be pooled
 */
class IChaosVDPooledObject
{
	GENERATED_BODY()

public:
	CHAOSVD_API virtual void OnAcquired() PURE_VIRTUAL(IChaosVDPooledObject::OnAcquired)
	CHAOSVD_API virtual void OnDisposed() PURE_VIRTUAL(IChaosVDPooledObject::OnDisposed)
};
