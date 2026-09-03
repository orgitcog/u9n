// Copyright Epic Games, Inc. All Rights Reserved.

#include "CogEngineManager.h"
#include "Engine/World.h"

ACogEngineManager::ACogEngineManager()
	: bSystemsInitialized(false)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f; // ~30 Hz
}

void ACogEngineManager::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Log, TEXT("CogEngineManager: Starting up"));
	InitializeAllSystems();
}

void ACogEngineManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (bSystemsInitialized)
	{
		ProcessCognitiveCycle(DeltaTime);
	}
}

void ACogEngineManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownAllSystems();
	Super::EndPlay(EndPlayReason);
}

void ACogEngineManager::InitializeAllSystems()
{
	UE_LOG(LogTemp, Log, TEXT("CogEngineManager: Initializing all cognitive systems"));

	// Initialize OpenCog Multi-Agent Orchestrator
	AgentOrchestrator = NewObject<UOpenCogOrchestrator>(this);
	if (AgentOrchestrator)
	{
		AgentOrchestrator->Initialize();
	}

	// Initialize Agent-Zero Distributed Kernel
	DistributedKernel = NewObject<UAgentZeroMicrokernel>(this);
	if (DistributedKernel)
	{
		DistributedKernel->Initialize(TEXT("MainKernel"));
	}

	// Initialize KoboldAI Storyweaver
	Storyweaver = NewObject<UAutognosticStoryweaver>(this);
	if (Storyweaver)
	{
		Storyweaver->Initialize();
	}

	// Initialize Deep-Tree-Echo Network
	EchoNetwork = NewObject<UEchoStateNetwork>(this);
	if (EchoNetwork)
	{
		EchoNetwork->InitializeNetwork(5, 100);
	}

	// Initialize Aphrodite LLM Engine
	LLMEngine = NewObject<ULLMInferenceEngine>(this);
	if (LLMEngine)
	{
		LLMEngine->Initialize(TEXT("http://localhost:2242"), TEXT(""));
	}

	// Initialize ROS Bridge
	RoboticsInterface = NewObject<UROSBridge>(this);
	if (RoboticsInterface)
	{
		RoboticsInterface->Initialize(TEXT("http://localhost:11311"), 9090);
	}

	// Initialize Home Assistant Bridge
	IoTInterface = NewObject<UHomeAssistantBridge>(this);
	if (IoTInterface)
	{
		IoTInterface->Initialize(TEXT("http://localhost:8123"), TEXT(""));
	}

	bSystemsInitialized = true;
	UE_LOG(LogTemp, Log, TEXT("CogEngineManager: All systems initialized successfully"));
}

void ACogEngineManager::ShutdownAllSystems()
{
	UE_LOG(LogTemp, Log, TEXT("CogEngineManager: Shutting down all cognitive systems"));

	if (AgentOrchestrator)
	{
		AgentOrchestrator->Shutdown();
	}

	if (EchoNetwork)
	{
		EchoNetwork->ResetNetwork();
	}

	if (RoboticsInterface)
	{
		RoboticsInterface->Disconnect();
	}

	if (IoTInterface)
	{
		IoTInterface->Disconnect();
	}

	bSystemsInitialized = false;
	UE_LOG(LogTemp, Log, TEXT("CogEngineManager: All systems shutdown complete"));
}

UOpenCogAgent* ACogEngineManager::CreateCognitiveAgent(const FString& AgentID)
{
	if (!AgentOrchestrator)
	{
		UE_LOG(LogTemp, Warning, TEXT("CogEngineManager: Orchestrator not initialized"));
		return nullptr;
	}

	UOpenCogAgent* NewAgent = NewObject<UOpenCogAgent>(this);
	if (NewAgent)
	{
		NewAgent->InitializeAgent(AgentID);
		
		if (AgentOrchestrator->RegisterAgent(NewAgent))
		{
			UE_LOG(LogTemp, Log, TEXT("CogEngineManager: Created and registered agent %s"), *AgentID);
			return NewAgent;
		}
	}

	return nullptr;
}

void ACogEngineManager::ProcessCognitiveCycle(float DeltaTime)
{
	// Orchestrate all agents
	if (AgentOrchestrator)
	{
		AgentOrchestrator->OrchestrateAgents(DeltaTime);
	}

	// Update distributed kernel
	if (DistributedKernel)
	{
		DistributedKernel->UpdateKernel(DeltaTime);
	}

	// Maintain story coherence
	if (Storyweaver && GetWorld()->GetTimeSeconds() - FMath::Floor(GetWorld()->GetTimeSeconds()) < DeltaTime)
	{
		// Run coherence check once per second
		Storyweaver->MaintainGlobalCoherence();
	}

	// Update echo-state network
	if (EchoNetwork)
	{
		EchoNetwork->UpdateNetworkState(DeltaTime);
	}
}

void ACogEngineManager::SubmitAgentReasoningRequest(const FString& AgentID, const FString& Context)
{
	if (!LLMEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("CogEngineManager: LLM Engine not initialized"));
		return;
	}

	FLLMInferenceRequest Request;
	Request.RequestID = FString::Printf(TEXT("%s_Reasoning_%d"), *AgentID, FMath::Rand());
	Request.Prompt = FString::Printf(TEXT("Agent %s needs to reason about: %s"), *AgentID, *Context);
	Request.MaxTokens = 256;
	Request.Temperature = 0.7f;

	LLMEngine->SubmitInferenceRequest(Request);
	UE_LOG(LogTemp, Log, TEXT("CogEngineManager: Submitted reasoning request for agent %s"), *AgentID);
}

FString ACogEngineManager::GetSystemStatus()
{
	FString Status = TEXT("CogEngine System Status:\n");
	
	if (AgentOrchestrator)
	{
		Status += FString::Printf(TEXT("- Agents: %d registered\n"), AgentOrchestrator->RegisteredAgents.Num());
	}
	
	if (Storyweaver)
	{
		Status += FString::Printf(TEXT("- Story Arcs: %d active\n"), Storyweaver->ActiveArcs.Num());
	}
	
	if (EchoNetwork)
	{
		Status += FString::Printf(TEXT("- Echo Network: %d nodes, depth %d\n"), 
			EchoNetwork->Nodes.Num(), EchoNetwork->NetworkDepth);
	}
	
	if (LLMEngine)
	{
		Status += FString::Printf(TEXT("- LLM Engine: %d models available\n"), LLMEngine->AvailableModels.Num());
	}
	
	if (RoboticsInterface)
	{
		Status += FString::Printf(TEXT("- ROS: %d topics subscribed\n"), RoboticsInterface->SubscribedTopics.Num());
	}
	
	if (IoTInterface)
	{
		Status += FString::Printf(TEXT("- IoT: %d devices registered\n"), IoTInterface->RegisteredDevices.Num());
	}
	
	return Status;
}
