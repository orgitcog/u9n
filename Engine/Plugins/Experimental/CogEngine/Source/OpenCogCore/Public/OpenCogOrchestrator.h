// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "OpenCogOrchestrator.generated.h"

class UOpenCogAgent;

/**
 * OpenCog Orchestrator - Central coordination for multi-agent system
 * Manages agent lifecycle, communication, and resource allocation
 */
UCLASS(Blueprintable, BlueprintType)
class OPENCOGCORE_API UOpenCogOrchestrator : public UObject
{
	GENERATED_BODY()

public:
	UOpenCogOrchestrator();

	/** All registered agents in the system */
	UPROPERTY(BlueprintReadOnly, Category = "OpenCog")
	TArray<UOpenCogAgent*> RegisteredAgents;

	/** Maximum number of agents allowed */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OpenCog")
	int32 MaxAgents;

	/** Register a new agent with the orchestrator */
	UFUNCTION(BlueprintCallable, Category = "OpenCog")
	bool RegisterAgent(UOpenCogAgent* Agent);

	/** Unregister an agent from the orchestrator */
	UFUNCTION(BlueprintCallable, Category = "OpenCog")
	bool UnregisterAgent(UOpenCogAgent* Agent);

	/** Get agent by ID */
	UFUNCTION(BlueprintCallable, Category = "OpenCog")
	UOpenCogAgent* GetAgentByID(const FString& AgentID);

	/** Orchestrate all agents - main coordination loop */
	UFUNCTION(BlueprintCallable, Category = "OpenCog")
	void OrchestrateAgents(float DeltaTime);

	/** Initialize the orchestrator */
	UFUNCTION(BlueprintCallable, Category = "OpenCog")
	void Initialize();

	/** Shutdown the orchestrator */
	UFUNCTION(BlueprintCallable, Category = "OpenCog")
	void Shutdown();

protected:
	/** Coordinate agent interactions */
	void CoordinateAgentInteractions();

	/** Allocate resources to agents */
	void AllocateResources();

	/** Resolve agent conflicts */
	void ResolveConflicts();

	bool bIsInitialized;
};
