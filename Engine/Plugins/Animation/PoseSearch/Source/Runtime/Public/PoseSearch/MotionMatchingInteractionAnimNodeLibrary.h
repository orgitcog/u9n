// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeReference.h"
#include "PoseSearch/PoseSearchInteractionLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MotionMatchingInteractionAnimNodeLibrary.generated.h"

#define UE_API POSESEARCH_API

struct FAnimNode_MotionMatchingInteraction;

USTRUCT(Experimental, BlueprintType)
struct FMotionMatchingInteractionAnimNodeReference : public FAnimNodeReference
{
	GENERATED_BODY()

	typedef FAnimNode_MotionMatchingInteraction FInternalNodeType;
};

UCLASS(MinimalAPI, Experimental)
class UMotionMatchingInteractionAnimNodeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Animation|Pose Search", meta = (BlueprintThreadSafe, ExpandEnumAsExecs = "Result"))
	static UE_API FMotionMatchingInteractionAnimNodeReference ConvertToMotionMatchingInteractionNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result);

	UFUNCTION(BlueprintPure, Category = "Animation|Pose Search", meta = (BlueprintThreadSafe, DisplayName = "Convert to Motion Matching Interaction Node"))
	static void ConvertToMotionMatchingInteractionNodePure(const FAnimNodeReference& Node, FMotionMatchingInteractionAnimNodeReference& MotionMatchingInteractionNode, bool& Result)
	{
		EAnimNodeReferenceConversionResult ConversionResult;
		MotionMatchingInteractionNode = ConvertToMotionMatchingInteractionNode(Node, ConversionResult);
		Result = (ConversionResult == EAnimNodeReferenceConversionResult::Succeeded);
	}

	UFUNCTION(BlueprintCallable, Category = "Animation|Pose Search", meta = (BlueprintThreadSafe))
	static UE_API void SetAvailabilities(const FMotionMatchingInteractionAnimNodeReference& MotionMatchingInteractionNode, const TArray<FPoseSearchInteractionAvailability>& Availabilities);

	UFUNCTION(BlueprintPure, Category = "Animation|Pose Search", meta = (BlueprintThreadSafe))
	static UE_API bool IsInteracting(const FMotionMatchingInteractionAnimNodeReference& MotionMatchingInteractionNode);
};

#undef UE_API
