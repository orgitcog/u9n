// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "OpenCogAgent.generated.h"

/**
 * Autonomous Agent for OpenCog multi-agent orchestration
 * Represents an individual cognitive agent with goals, beliefs, and actions
 */
UCLASS(Blueprintable, BlueprintType)
class OPENCOGCORE_API UOpenCogAgent : public UObject
{
	GENERATED_BODY()

public:
	UOpenCogAgent();

	/** Agent identifier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OpenCog")
	FString AgentID;

	/** Current agent state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OpenCog")
	FString AgentState;

	/** Agent's current goals */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OpenCog")
	TArray<FString> Goals;

	/** Agent's belief system */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OpenCog")
	TMap<FString, float> Beliefs;

	/** Initialize agent with ID */
	UFUNCTION(BlueprintCallable, Category = "OpenCog")
	void InitializeAgent(const FString& InAgentID);

	/** Add a goal to the agent */
	UFUNCTION(BlueprintCallable, Category = "OpenCog")
	void AddGoal(const FString& Goal);

	/** Update agent beliefs */
	UFUNCTION(BlueprintCallable, Category = "OpenCog")
	void UpdateBelief(const FString& BeliefKey, float BeliefValue);

	/** Execute agent cognitive cycle */
	UFUNCTION(BlueprintCallable, Category = "OpenCog")
	void ExecuteCognitiveCycle(float DeltaTime);

protected:
	/** Process goals and select actions */
	void ProcessGoals();

	/** Update internal state based on beliefs */
	void UpdateState();
};
