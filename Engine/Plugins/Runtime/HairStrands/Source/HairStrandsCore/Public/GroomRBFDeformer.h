// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define UE_API HAIRSTRANDSCORE_API

class UGroomAsset;
class UGroomBindingAsset;
struct FTextureSource;

struct FGroomRBFDeformer
{
	// Return a new GroomAsset with the RBF deformation from the BindingAsset baked into it
	UE_API void GetRBFDeformedGroomAsset(const UGroomAsset* InGroomAsset, const UGroomBindingAsset* BindingAsset, FTextureSource* MaskTextureSource, const float MaskScale, UGroomAsset* OutGroomAsset, const ITargetPlatform* TargetPlatform=nullptr);

	static UE_API uint32 GetEntryCount(uint32 InSampleCount);
	static UE_API uint32 GetWeightCount(uint32 InSampleCount);
};

#undef UE_API
