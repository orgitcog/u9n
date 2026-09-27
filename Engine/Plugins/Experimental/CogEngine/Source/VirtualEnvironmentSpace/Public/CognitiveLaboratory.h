// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CognitiveLaboratory.generated.h"

class UOpenCogOrchestrator;
class UAgentZeroMicrokernel;
class UAutognosticStoryweaver;
class UEchoStateNetwork;
class ULLMInferenceEngine;
class UROSBridge;
class UHomeAssistantBridge;

/**
 * Cognitive Laboratory Component
 * Integrates all OpenCog subsystems into a unified cognitive workspace
 * Inspired by aiangel.io's integrated AI environment
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class VIRTUALENVIRONMENTSPACE_API UCognitiveLaboratory : public UActorComponent
{
	GENERATED_BODY()

public:
	UCognitiveLaboratory();

	/** OpenCog multi-agent orchestrator */
	UPROPERTY(BlueprintReadOnly, Category = "Cognitive Laboratory")
	UOpenCogOrchestrator* Orchestrator;

	/** Agent-Zero distributed cognitive kernel */
	UPROPERTY(BlueprintReadOnly, Category = "Cognitive Laboratory")
	UAgentZeroMicrokernel* DistributedKernel;

	/** KoboldAI narrative storyweaver */
	UPROPERTY(BlueprintReadOnly, Category = "Cognitive Laboratory")
	UAutognosticStoryweaver* Storyweaver;

	/** Deep-Tree-Echo embodied cognition network */
	UPROPERTY(BlueprintReadOnly, Category = "Cognitive Laboratory")
	UEchoStateNetwork* EchoNetwork;

	/** Aphrodite LLM inference engine */
	UPROPERTY(BlueprintReadOnly, Category = "Cognitive Laboratory")
	ULLMInferenceEngine* LLMEngine;

	/** ROS robotics bridge */
	UPROPERTY(BlueprintReadOnly, Category = "Cognitive Laboratory")
	UROSBridge* RoboticsInterface;

	/** Home Assistant IoT bridge */
	UPROPERTY(BlueprintReadOnly, Category = "Cognitive Laboratory")
	UHomeAssistantBridge* IoTInterface;

	/** Is the laboratory currently active */
	UPROPERTY(BlueprintReadOnly, Category = "Cognitive Laboratory")
	bool bIsLaboratoryActive;

	/** Laboratory name/identifier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cognitive Laboratory")
	FString LaboratoryName;

	/** Initialize all cognitive subsystems */
	UFUNCTION(BlueprintCallable, Category = "Cognitive Laboratory")
	void InitializeLaboratory();

	/** Shutdown all cognitive subsystems */
	UFUNCTION(BlueprintCallable, Category = "Cognitive Laboratory")
	void ShutdownLaboratory();

	/** Enable/disable specific subsystem */
	UFUNCTION(BlueprintCallable, Category = "Cognitive Laboratory")
	void SetSubsystemEnabled(const FString& SubsystemName, bool bEnabled);

	/** Get status of all subsystems */
	UFUNCTION(BlueprintCallable, Category = "Cognitive Laboratory")
	TMap<FString, bool> GetSubsystemStatus() const;

	/** Process cognitive cycle for all active subsystems */
	UFUNCTION(BlueprintCallable, Category = "Cognitive Laboratory")
	void ProcessCognitiveCycle(float DeltaTime);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Subsystem enabled flags */
	TMap<FString, bool> SubsystemEnabled;

	/** Initialize individual subsystems */
	void InitializeOrchestrator();
	void InitializeDistributedKernel();
	void InitializeStoryweaver();
	void InitializeEchoNetwork();
	void InitializeLLMEngine();
	void InitializeRoboticsInterface();
	void InitializeIoTInterface();
};
