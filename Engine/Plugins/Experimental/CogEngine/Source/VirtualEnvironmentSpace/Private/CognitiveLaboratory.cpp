// Copyright Epic Games, Inc. All Rights Reserved.

#include "CognitiveLaboratory.h"
#include "OpenCogOrchestrator.h"
#include "AgentZeroMicrokernel.h"
#include "AutognosticStoryweaver.h"
#include "EchoStateNetwork.h"
#include "LLMInferenceEngine.h"
#include "ROSBridge.h"
#include "HomeAssistantBridge.h"

UCognitiveLaboratory::UCognitiveLaboratory()
	: bIsLaboratoryActive(false)
	, LaboratoryName(TEXT("OpenCog Cognitive Laboratory"))
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCognitiveLaboratory::BeginPlay()
{
	Super::BeginPlay();
	InitializeLaboratory();
}

void UCognitiveLaboratory::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (bIsLaboratoryActive)
	{
		ProcessCognitiveCycle(DeltaTime);
	}
}

void UCognitiveLaboratory::InitializeLaboratory()
{
	if (bIsLaboratoryActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("CognitiveLaboratory: %s already initialized"), *LaboratoryName);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: Initializing %s"), *LaboratoryName);

	// Initialize all subsystems
	InitializeOrchestrator();
	InitializeDistributedKernel();
	InitializeStoryweaver();
	InitializeEchoNetwork();
	InitializeLLMEngine();
	InitializeRoboticsInterface();
	InitializeIoTInterface();

	bIsLaboratoryActive = true;
	UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: %s fully initialized"), *LaboratoryName);
}

void UCognitiveLaboratory::ShutdownLaboratory()
{
	if (!bIsLaboratoryActive)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: Shutting down %s"), *LaboratoryName);

	// Shutdown all subsystems
	if (Orchestrator)
	{
		Orchestrator->Shutdown();
	}

	if (DistributedKernel)
	{
		DistributedKernel->LeaveNetwork();
	}

	if (Storyweaver)
	{
		Storyweaver->Shutdown();
	}

	if (LLMEngine)
	{
		LLMEngine->Shutdown();
	}

	if (RoboticsInterface)
	{
		RoboticsInterface->Disconnect();
	}

	if (IoTInterface)
	{
		IoTInterface->Disconnect();
	}

	bIsLaboratoryActive = false;
	SubsystemEnabled.Empty();

	UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: %s shutdown complete"), *LaboratoryName);
}

void UCognitiveLaboratory::SetSubsystemEnabled(const FString& SubsystemName, bool bEnabled)
{
	SubsystemEnabled.Add(SubsystemName, bEnabled);
	UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: Subsystem %s %s"), 
		*SubsystemName, bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

TMap<FString, bool> UCognitiveLaboratory::GetSubsystemStatus() const
{
	TMap<FString, bool> Status;
	Status.Add(TEXT("Orchestrator"), Orchestrator != nullptr && SubsystemEnabled.FindRef(TEXT("Orchestrator")));
	Status.Add(TEXT("DistributedKernel"), DistributedKernel != nullptr && SubsystemEnabled.FindRef(TEXT("DistributedKernel")));
	Status.Add(TEXT("Storyweaver"), Storyweaver != nullptr && SubsystemEnabled.FindRef(TEXT("Storyweaver")));
	Status.Add(TEXT("EchoNetwork"), EchoNetwork != nullptr && SubsystemEnabled.FindRef(TEXT("EchoNetwork")));
	Status.Add(TEXT("LLMEngine"), LLMEngine != nullptr && SubsystemEnabled.FindRef(TEXT("LLMEngine")));
	Status.Add(TEXT("RoboticsInterface"), RoboticsInterface != nullptr && SubsystemEnabled.FindRef(TEXT("RoboticsInterface")));
	Status.Add(TEXT("IoTInterface"), IoTInterface != nullptr && SubsystemEnabled.FindRef(TEXT("IoTInterface")));
	return Status;
}

void UCognitiveLaboratory::ProcessCognitiveCycle(float DeltaTime)
{
	// Process OpenCog orchestration
	if (Orchestrator && SubsystemEnabled.FindRef(TEXT("Orchestrator")))
	{
		Orchestrator->OrchestrateAgents(DeltaTime);
	}

	// Process narrative coherence
	if (Storyweaver && SubsystemEnabled.FindRef(TEXT("Storyweaver")))
	{
		Storyweaver->MaintainGlobalCoherence();
	}
}

void UCognitiveLaboratory::InitializeOrchestrator()
{
	Orchestrator = NewObject<UOpenCogOrchestrator>(this);
	if (Orchestrator)
	{
		Orchestrator->Initialize();
		SubsystemEnabled.Add(TEXT("Orchestrator"), true);
		UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: OpenCog Orchestrator initialized"));
	}
}

void UCognitiveLaboratory::InitializeDistributedKernel()
{
	DistributedKernel = NewObject<UAgentZeroMicrokernel>(this);
	if (DistributedKernel)
	{
		DistributedKernel->Initialize(TEXT("CogLab_Kernel"));
		SubsystemEnabled.Add(TEXT("DistributedKernel"), true);
		UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: Agent-Zero Distributed Kernel initialized"));
	}
}

void UCognitiveLaboratory::InitializeStoryweaver()
{
	Storyweaver = NewObject<UAutognosticStoryweaver>(this);
	if (Storyweaver)
	{
		Storyweaver->Initialize();
		SubsystemEnabled.Add(TEXT("Storyweaver"), true);
		UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: KoboldAI Storyweaver initialized"));
	}
}

void UCognitiveLaboratory::InitializeEchoNetwork()
{
	EchoNetwork = NewObject<UEchoStateNetwork>(this);
	if (EchoNetwork)
	{
		EchoNetwork->InitializeNetwork(5, 100); // Default: 5 depth, 100 reservoir size
		SubsystemEnabled.Add(TEXT("EchoNetwork"), true);
		UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: Deep-Tree-Echo Network initialized"));
	}
}

void UCognitiveLaboratory::InitializeLLMEngine()
{
	LLMEngine = NewObject<ULLMInferenceEngine>(this);
	if (LLMEngine)
	{
		// Initialize with default local server
		LLMEngine->Initialize(TEXT("http://localhost:2242"), TEXT("default_api_key"));
		SubsystemEnabled.Add(TEXT("LLMEngine"), true);
		UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: Aphrodite LLM Engine initialized"));
	}
}

void UCognitiveLaboratory::InitializeRoboticsInterface()
{
	RoboticsInterface = NewObject<UROSBridge>(this);
	if (RoboticsInterface)
	{
		// Initialize with default ROS master
		RoboticsInterface->Initialize(TEXT("http://localhost:11311"), 9090);
		SubsystemEnabled.Add(TEXT("RoboticsInterface"), true);
		UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: ROS Robotics Interface initialized"));
	}
}

void UCognitiveLaboratory::InitializeIoTInterface()
{
	IoTInterface = NewObject<UHomeAssistantBridge>(this);
	if (IoTInterface)
	{
		// Initialize with default Home Assistant
		IoTInterface->Initialize(TEXT("http://localhost:8123"), TEXT("default_token"));
		SubsystemEnabled.Add(TEXT("IoTInterface"), true);
		UE_LOG(LogTemp, Log, TEXT("CognitiveLaboratory: Home Assistant IoT Interface initialized"));
	}
}
