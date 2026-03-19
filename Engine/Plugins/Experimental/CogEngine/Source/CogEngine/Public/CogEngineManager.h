// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OpenCogOrchestrator.h"
#include "OpenCogAgent.h"
#include "AgentZeroMicrokernel.h"
#include "AutognosticStoryweaver.h"
#include "EchoStateNetwork.h"
#include "LLMInferenceEngine.h"
#include "ROSBridge.h"
#include "HomeAssistantBridge.h"
#include "CogEngineManager.generated.h"

/**
 * CogEngine Manager - Central manager for all cognitive systems
 * This actor demonstrates integration of all CogEngine components
 */
UCLASS(Blueprintable, BlueprintType)
class COGENGINE_API ACogEngineManager : public AActor
{
	GENERATED_BODY()
	
public:	
	ACogEngineManager();

	/** OpenCog Multi-Agent Orchestrator */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CogEngine")
	UOpenCogOrchestrator* AgentOrchestrator;

	/** Agent-Zero Distributed Microkernel */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CogEngine")
	UAgentZeroMicrokernel* DistributedKernel;

	/** KoboldAI Autognostic Storyweaver */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CogEngine")
	UAutognosticStoryweaver* Storyweaver;

	/** Deep-Tree-Echo Network */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CogEngine")
	UEchoStateNetwork* EchoNetwork;

	/** Aphrodite LLM Inference Engine */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CogEngine")
	ULLMInferenceEngine* LLMEngine;

	/** ROS Bridge for Robotics */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CogEngine")
	UROSBridge* RoboticsInterface;

	/** Home Assistant IoT Bridge */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CogEngine")
	UHomeAssistantBridge* IoTInterface;

	/** Initialize all cognitive systems */
	UFUNCTION(BlueprintCallable, Category = "CogEngine")
	void InitializeAllSystems();

	/** Shutdown all cognitive systems */
	UFUNCTION(BlueprintCallable, Category = "CogEngine")
	void ShutdownAllSystems();

	/** Create a new cognitive agent */
	UFUNCTION(BlueprintCallable, Category = "CogEngine")
	UOpenCogAgent* CreateCognitiveAgent(const FString& AgentID);

	/** Process cognitive cycle for all systems */
	UFUNCTION(BlueprintCallable, Category = "CogEngine")
	void ProcessCognitiveCycle(float DeltaTime);

	/** Submit LLM inference request for agent reasoning */
	UFUNCTION(BlueprintCallable, Category = "CogEngine")
	void SubmitAgentReasoningRequest(const FString& AgentID, const FString& Context);

	/** Get system status report */
	UFUNCTION(BlueprintCallable, Category = "CogEngine")
	FString GetSystemStatus();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bSystemsInitialized;
};
