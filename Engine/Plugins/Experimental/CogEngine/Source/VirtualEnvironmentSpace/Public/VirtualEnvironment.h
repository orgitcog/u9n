// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualEnvironment.generated.h"

class UOpenCogOrchestrator;
class UOpenCogAgent;

/**
 * Virtual Environment Actor
 * Represents a 3D virtual space where cognitive agents can operate
 * Similar to aiangel.io virtual environment concept
 */
UCLASS(Blueprintable, BlueprintType)
class VIRTUALENVIRONMENTSPACE_API AVirtualEnvironment : public AActor
{
	GENERATED_BODY()

public:
	AVirtualEnvironment();

	/** Name of this virtual environment */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Virtual Environment")
	FString EnvironmentName;

	/** Description of the environment's purpose */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Virtual Environment")
	FString Description;

	/** Size of the virtual space (in Unreal units) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Virtual Environment")
	FVector EnvironmentSize;

	/** Is this environment currently active */
	UPROPERTY(BlueprintReadOnly, Category = "Virtual Environment")
	bool bIsActive;

	/** Reference to the OpenCog orchestrator managing agents in this environment */
	UPROPERTY(BlueprintReadWrite, Category = "Virtual Environment")
	UOpenCogOrchestrator* CognitiveOrchestrator;

	/** All agents currently operating in this environment */
	UPROPERTY(BlueprintReadOnly, Category = "Virtual Environment")
	TArray<UOpenCogAgent*> EnvironmentAgents;

	/** Initialize the virtual environment */
	UFUNCTION(BlueprintCallable, Category = "Virtual Environment")
	void InitializeEnvironment();

	/** Shutdown the virtual environment */
	UFUNCTION(BlueprintCallable, Category = "Virtual Environment")
	void ShutdownEnvironment();

	/** Add an agent to this virtual environment */
	UFUNCTION(BlueprintCallable, Category = "Virtual Environment")
	bool AddAgent(UOpenCogAgent* Agent, FVector SpawnLocation);

	/** Remove an agent from this virtual environment */
	UFUNCTION(BlueprintCallable, Category = "Virtual Environment")
	bool RemoveAgent(UOpenCogAgent* Agent);

	/** Get a random location within the environment bounds */
	UFUNCTION(BlueprintCallable, Category = "Virtual Environment")
	FVector GetRandomLocation() const;

	/** Check if a location is within environment bounds */
	UFUNCTION(BlueprintCallable, Category = "Virtual Environment")
	bool IsLocationInBounds(FVector Location) const;

	/** Update environment state (called every frame) */
	UFUNCTION(BlueprintCallable, Category = "Virtual Environment")
	void UpdateEnvironment(float DeltaTime);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	/** Cached center of the environment */
	FVector EnvironmentCenter;
};
