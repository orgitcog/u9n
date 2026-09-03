// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Box2D.h"
#include "MaterialCacheStack.h"
#include "MaterialCacheStackProvider.generated.h"

UCLASS(MinimalAPI, abstract)
class UMaterialCacheStackProvider : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Evaluate the material stack of a given uv-range.
	 * Called on the render thread.
	 */
	virtual void Evaluate(const FBox2f& UVRect, FMaterialCacheStack* OutStack)
	{
		checkNoEntry();
	}

#if WITH_EDITOR
	/**
	 * Called prior to stack evaluation to check if all relevant resources are ready.
	 * Called on the render thread.
	 */
	virtual bool IsMaterialResourcesReady()
	{
		return true;
	}
#endif // WITH_EDITOR
};
