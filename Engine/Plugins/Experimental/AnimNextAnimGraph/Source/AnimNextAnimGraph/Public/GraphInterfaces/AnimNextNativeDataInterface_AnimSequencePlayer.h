// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterface/AnimNextNativeDataInterface.h"
#include "AnimNextNativeDataInterface_AnimSequencePlayer.generated.h"

class UAnimSequence;

/**
* Native interface to built-in AnimSequence player graph at: /AnimNextAnimGraph/FactoryGraphs/AG_AnimSequencePlayer
 */
USTRUCT(BlueprintType, DisplayName = "UAF Anim Sequence Player Data")
struct FAnimNextNativeDataInterface_AnimSequencePlayer : public FAnimNextNativeDataInterface
{
	GENERATED_BODY()

	// FAnimNextNativeDataInterface interface
	ANIMNEXTANIMGRAPH_API virtual void BindToFactoryObject(const FBindToFactoryObjectContext& InContext) override;

	// The animation object to play
	UPROPERTY(BlueprintReadWrite, Category = "Anim Sequence")	// Not editable to hide it in cases where we would end up duplicating factory source asset
	TObjectPtr<const UAnimSequence> AnimSequence;

	// The play rate to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim Sequence", meta = (EnableCategories))
	double PlayRate = 1.0f;

	// The timeline's start position
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim Sequence", meta = (EnableCategories))
	double StartPosition = 0.0f;

	// Whether to loop the animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim Sequence", meta = (EnableCategories))
	bool Loop = false;
};
