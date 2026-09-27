// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LearningAgentsLearningComponentInterface.generated.h"

/**
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint), BlueprintType)
class ULearningAgentsLearningComponentInterface : public UInterface
{
	GENERATED_BODY()
};

class LEARNINGAGENTSTRAINING_API ILearningAgentsLearningComponentInterface
{
	GENERATED_BODY()

public:
	/** Initializes the component at the start of training. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	virtual void InitializeLearningComponent() = 0;

	/** Resets the component for a new training episode. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	virtual void ResetLearningComponent() = 0;
};
