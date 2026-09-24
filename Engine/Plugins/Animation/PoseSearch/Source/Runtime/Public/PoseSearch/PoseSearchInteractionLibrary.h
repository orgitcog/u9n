// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "PoseSearch/PoseSearchInteractionIsland.h"
#include "PoseSearchInteractionLibrary.generated.h"

#define UE_API POSESEARCH_API

// Experimental, this feature might be removed without warning, not for production use
UCLASS(MinimalAPI, Experimental)
class UPoseSearchInteractionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// function publishing this character (via its AnimInstance) FPoseSearchInteractionAvailability to the UPoseSearchInteractionSubsystem,
	// FPoseSearchInteractionAvailability represents the character availability to partecipate in an interaction with other characters for the next frame.
	// that means there will always be one frame delay between publiching availabilities and getting a result back from MotionMatchInteraction_Pure!
	// 
	// if FPoseSearchBlueprintResult has a valid SelectedAnimation, this will be the animation assigned to this character to partecipate in this interaction.
	// additional interaction properties, like assigned role, SelectedAnimation time, SearchCost, etc can be found within the result
	// ContinuingProperties are used to figure out the continuing pose and bias it accordingly. ContinuingProperties can reference directly the UMultiAnimAsset
	// or any of the roled UMultiAnimAsset::GetAnimationAsset, and the UPoseSearchInteractionSubsystem will figure out the related UMultiAnimAsset
	// PoseHistoryName is the name of the pose history node used for the associated motion matching search
	// if bValidateResultAgainstAvailabilities is true, the result will be invalidated if doesn't respect the new availabilities
	UFUNCTION(Experimental, BlueprintPure, Category = "Animation|Pose Search", meta = (BlueprintThreadSafe))
	static UE_API FPoseSearchBlueprintResult MotionMatchInteraction_Pure(TArray<FPoseSearchInteractionAvailability> Availabilities, const UObject* AnimContext, FName PoseHistoryName, bool bValidateResultAgainstAvailabilities = true);

	// BlueprintCallable version of MotionMatchInteraction_Pure
	UFUNCTION(Experimental, BlueprintCallable, Category = "Animation|Pose Search", meta = (BlueprintThreadSafe))
	static UE_API FPoseSearchBlueprintResult MotionMatchInteraction(TArray<FPoseSearchInteractionAvailability> Availabilities, const UObject* AnimContext, FName PoseHistoryName, bool bValidateResultAgainstAvailabilities = true);

	static UE_API void MotionMatchInteraction(FPoseSearchBlueprintResult& Result, const TArrayView<const FPoseSearchInteractionAvailability> Availabilities, const UObject* AnimContext, FName PoseHistoryName, const UE::PoseSearch::IPoseHistory* PoseHistory, bool bValidateResultAgainstAvailabilities = true);

	static UE_API FPoseSearchBlueprintResult GetCachedInteraction(const UObject* AnimContext, bool bCompareOwningActors = false);
	
	UFUNCTION(Experimental, BlueprintPure, Category = "Animation|Pose Search", meta = (BlueprintThreadSafe))
	static UE_API FPoseSearchContinuingProperties GetMontageContinuingProperties(UAnimInstance* AnimInstance);
};

#undef UE_API
