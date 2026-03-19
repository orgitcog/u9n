// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "ITextureShareDisplayClusterContext.h"

/**
* Custom implementation of TextureShare context for nDisplay
*/
struct FTextureSharePostprocessContext
	: public ITextureShareDisplayClusterContext
{
	// ~Begin ITextureShareDisplayClusterContext
	virtual ~FTextureSharePostprocessContext() = default;	
	// ~~ End ITextureShareDisplayClusterContext
};
