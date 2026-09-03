// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterface/AnimNextNativeDataInterface.h"
#include "AnimNextNativeDataInterface_BlendSpacePlayer.generated.h"

class UBlendSpace;

/**
* Native interface to built-in BlendSpace player graph at: /AnimNextAnimGraph/FactoryGraphs/AG_BlendSpacePlayer
 */
USTRUCT(BlueprintType, DisplayName = "UAF Blend Space Player Data")
struct FAnimNextNativeDataInterface_BlendSpacePlayer : public FAnimNextNativeDataInterface
{
	GENERATED_BODY()

	// FAnimNextNativeDataInterface interface
	ANIMNEXTANIMGRAPH_API virtual void BindToFactoryObject(const FBindToFactoryObjectContext& InContext) override;

	// The animation object to play
	UPROPERTY(BlueprintReadWrite, Category = "Blend Space")	// Not editable to hide it in cases where we would end up duplicating factory source asset
	TObjectPtr<const UBlendSpace> BlendSpace;

	// The location on the x-axis to sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend Space", meta = (EnableCategories))
	double XAxisSamplePoint = 0.0f;

	// The location on the y-axis to sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend Space", meta = (EnableCategories))
	double YAxisSamplePoint = 0.0f;

	// The play rate to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend Space", meta = (EnableCategories))
	double PlayRate = 1.0f;

	// The timeline's start position. This is normalized in the [0,1] range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend Space", meta = (EnableCategories))
	double StartPosition = 0.0f;

	// Whether to loop the animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend Space", meta = (EnableCategories))
	bool Loop = false;
};
