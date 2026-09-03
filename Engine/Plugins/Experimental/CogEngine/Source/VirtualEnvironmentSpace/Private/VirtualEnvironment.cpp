// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualEnvironment.h"
#include "OpenCogOrchestrator.h"
#include "OpenCogAgent.h"

AVirtualEnvironment::AVirtualEnvironment()
	: EnvironmentName(TEXT("Cognitive Laboratory"))
	, Description(TEXT("Virtual environment for autonomous cognitive agents"))
	, EnvironmentSize(10000.0f, 10000.0f, 5000.0f)
	, bIsActive(false)
	, CognitiveOrchestrator(nullptr)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVirtualEnvironment::BeginPlay()
{
	Super::BeginPlay();
	EnvironmentCenter = GetActorLocation();
	InitializeEnvironment();
}

void AVirtualEnvironment::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (bIsActive)
	{
		UpdateEnvironment(DeltaTime);
	}
}

void AVirtualEnvironment::InitializeEnvironment()
{
	if (bIsActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("VirtualEnvironment: %s already initialized"), *EnvironmentName);
		return;
	}

	bIsActive = true;
	EnvironmentCenter = GetActorLocation();
	EnvironmentAgents.Empty();

	// Create orchestrator if not already set
	if (!CognitiveOrchestrator)
	{
		CognitiveOrchestrator = NewObject<UOpenCogOrchestrator>(this);
		CognitiveOrchestrator->Initialize();
	}

	UE_LOG(LogTemp, Log, TEXT("VirtualEnvironment: %s initialized at location %s with size %s"), 
		*EnvironmentName, *EnvironmentCenter.ToString(), *EnvironmentSize.ToString());
}

void AVirtualEnvironment::ShutdownEnvironment()
{
	if (!bIsActive)
	{
		return;
	}

	bIsActive = false;

	// Cleanup orchestrator
	if (CognitiveOrchestrator)
	{
		CognitiveOrchestrator->Shutdown();
	}

	EnvironmentAgents.Empty();

	UE_LOG(LogTemp, Log, TEXT("VirtualEnvironment: %s shutdown complete"), *EnvironmentName);
}

bool AVirtualEnvironment::AddAgent(UOpenCogAgent* Agent, FVector SpawnLocation)
{
	if (!Agent || !bIsActive)
	{
		return false;
	}

	// Check if location is within bounds
	if (!IsLocationInBounds(SpawnLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("VirtualEnvironment: Spawn location %s is outside bounds"), *SpawnLocation.ToString());
		return false;
	}

	// Register with orchestrator
	if (CognitiveOrchestrator && CognitiveOrchestrator->RegisterAgent(Agent))
	{
		EnvironmentAgents.Add(Agent);
		UE_LOG(LogTemp, Log, TEXT("VirtualEnvironment: Agent %s added at location %s"), 
			*Agent->AgentID, *SpawnLocation.ToString());
		return true;
	}

	return false;
}

bool AVirtualEnvironment::RemoveAgent(UOpenCogAgent* Agent)
{
	if (!Agent)
	{
		return false;
	}

	// Unregister from orchestrator
	if (CognitiveOrchestrator)
	{
		CognitiveOrchestrator->UnregisterAgent(Agent);
	}

	int32 Removed = EnvironmentAgents.Remove(Agent);
	if (Removed > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("VirtualEnvironment: Agent %s removed"), *Agent->AgentID);
		return true;
	}

	return false;
}

FVector AVirtualEnvironment::GetRandomLocation() const
{
	FVector RandomOffset;
	RandomOffset.X = FMath::RandRange(-EnvironmentSize.X / 2.0f, EnvironmentSize.X / 2.0f);
	RandomOffset.Y = FMath::RandRange(-EnvironmentSize.Y / 2.0f, EnvironmentSize.Y / 2.0f);
	RandomOffset.Z = FMath::RandRange(-EnvironmentSize.Z / 2.0f, EnvironmentSize.Z / 2.0f);
	
	return EnvironmentCenter + RandomOffset;
}

bool AVirtualEnvironment::IsLocationInBounds(FVector Location) const
{
	FVector Delta = Location - EnvironmentCenter;
	
	return FMath::Abs(Delta.X) <= EnvironmentSize.X / 2.0f &&
	       FMath::Abs(Delta.Y) <= EnvironmentSize.Y / 2.0f &&
	       FMath::Abs(Delta.Z) <= EnvironmentSize.Z / 2.0f;
}

void AVirtualEnvironment::UpdateEnvironment(float DeltaTime)
{
	// Run orchestrator cognitive cycle for all agents
	if (CognitiveOrchestrator)
	{
		CognitiveOrchestrator->OrchestrateAgents(DeltaTime);
	}
}
