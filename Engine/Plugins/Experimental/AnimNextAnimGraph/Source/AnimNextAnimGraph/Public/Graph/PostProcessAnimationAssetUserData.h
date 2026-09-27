// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"

#include "PostProcessAnimationAssetUserData.generated.h"

/** Asset user data used to store post-process animation data for a skeletal mesh asset. */
UCLASS(MinimalAPI, meta = (DisplayName = "Post-process Animation User Data"))
class UPostProcessAnimationUserAssetData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Animation)
	TObjectPtr<UObject> AnimationAsset;
};