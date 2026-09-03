// Copyright Epic Games, Inc. All Rights Reserved.

#include "PyperNeuroBattleDemo.h"
#include "Engine/World.h"

APyperNeuroBattleDemo::APyperNeuroBattleDemo()
	: Pyper(nullptr)
	, Neuro(nullptr)
	, BattleSystem(nullptr)
	, bAutoStartBattle(true)
	, PyperModelPath(TEXT("/Game/Models/Pyper.model3.json"))
	, NeuroTypeToSpawn(ENeuroType::Advanced)
	, bDemoActive(false)
{
	PrimaryActorTick.bCanEverTick = true;
}

void APyperNeuroBattleDemo::BeginPlay()
{
	Super::BeginPlay();
	
	if (bAutoStartBattle)
	{
		InitializeDemo();
		StartDemo();
	}
}

void APyperNeuroBattleDemo::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDemoActive)
	{
		UpdateDemo(DeltaTime);
	}
}

void APyperNeuroBattleDemo::InitializeDemo()
{
	UE_LOG(LogTemp, Log, TEXT("PyperNeuroBattleDemo: Initializing demo..."));
	
	SpawnEntities();
	
	if (Pyper && Neuro && BattleSystem)
	{
		BattleSystem->InitializeBattle(Pyper, Neuro);
		UE_LOG(LogTemp, Log, TEXT("PyperNeuroBattleDemo: Demo initialized successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PyperNeuroBattleDemo: Failed to initialize - missing entities"));
	}
}

void APyperNeuroBattleDemo::StartDemo()
{
	if (!BattleSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("PyperNeuroBattleDemo: Cannot start - battle system not initialized"));
		return;
	}

	BattleSystem->StartBattle();
	bDemoActive = true;
	
	UE_LOG(LogTemp, Log, TEXT("PyperNeuroBattleDemo: Demo started - Pyper vs Neuro battle begins!"));
}

void APyperNeuroBattleDemo::StopDemo()
{
	bDemoActive = false;
	
	if (BattleSystem)
	{
		BattleSystem->SetSuperhotMode(false);
	}
	
	UE_LOG(LogTemp, Log, TEXT("PyperNeuroBattleDemo: Demo stopped"));
}

FString APyperNeuroBattleDemo::GetBattleStatus() const
{
	if (!BattleSystem)
	{
		return TEXT("Battle system not initialized");
	}

	FString Status;
	
	switch (BattleSystem->BattleState)
	{
		case EBattleState::Idle:
			Status = TEXT("Idle - Waiting to start");
			break;
		case EBattleState::Preparing:
			Status = TEXT("Preparing - Setting up battle");
			break;
		case EBattleState::Fighting:
			Status = FString::Printf(TEXT("Fighting - Pyper: %.1f | Neuro: %.1f"), 
				BattleSystem->Result.PyperScore, 
				BattleSystem->Result.NeuroScore);
			break;
		case EBattleState::Fusing:
			Status = TEXT("Fusing - Creating fusion entity");
			break;
		case EBattleState::Complete:
			if (BattleSystem->Result.bWasFusion)
			{
				Status = FString::Printf(TEXT("Complete - FUSION! Power: %.1f"), 
					BattleSystem->Result.FusionPower);
			}
			else
			{
				Status = FString::Printf(TEXT("Complete - Winner: %s"), 
					*BattleSystem->Result.WinnerName);
			}
			break;
		default:
			Status = TEXT("Unknown state");
			break;
	}

	return Status;
}

void APyperNeuroBattleDemo::SpawnEntities()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Spawn Pyper
	FVector PyperLocation = GetActorLocation() + FVector(-200.0f, 0.0f, 0.0f);
	FRotator PyperRotation = FRotator::ZeroRotator;
	
	Pyper = World->SpawnActor<APyperActor>(APyperActor::StaticClass(), PyperLocation, PyperRotation);
	if (Pyper)
	{
		Pyper->InitializePyper(PyperModelPath);
		UE_LOG(LogTemp, Log, TEXT("PyperNeuroBattleDemo: Pyper spawned"));
	}

	// Spawn Neuro
	FVector NeuroLocation = GetActorLocation() + FVector(200.0f, 0.0f, 0.0f);
	FRotator NeuroRotation = FRotator::ZeroRotator;
	
	Neuro = World->SpawnActor<ANeuroEntity>(ANeuroEntity::StaticClass(), NeuroLocation, NeuroRotation);
	if (Neuro)
	{
		Neuro->InitializeNeuro(NeuroTypeToSpawn);
		UE_LOG(LogTemp, Log, TEXT("PyperNeuroBattleDemo: Neuro spawned"));
	}

	// Spawn Battle System
	FVector BattleLocation = GetActorLocation();
	FRotator BattleRotation = FRotator::ZeroRotator;
	
	BattleSystem = World->SpawnActor<ASuperhotFusionBattle>(
		ASuperhotFusionBattle::StaticClass(), 
		BattleLocation, 
		BattleRotation
	);
	
	if (BattleSystem)
	{
		UE_LOG(LogTemp, Log, TEXT("PyperNeuroBattleDemo: Battle system spawned"));
	}
}

void APyperNeuroBattleDemo::UpdateDemo(float DeltaTime)
{
	// Monitor battle state and log important events
	if (BattleSystem)
	{
		static EBattleState LastState = EBattleState::Idle;
		
		if (BattleSystem->BattleState != LastState)
		{
			UE_LOG(LogTemp, Log, TEXT("PyperNeuroBattleDemo: Battle state changed - %s"), 
				*GetBattleStatus());
			LastState = BattleSystem->BattleState;
		}

		// Auto-stop when complete
		if (BattleSystem->BattleState == EBattleState::Complete && bDemoActive)
		{
			UE_LOG(LogTemp, Log, TEXT("PyperNeuroBattleDemo: Battle complete! Final status: %s"), 
				*GetBattleStatus());
			bDemoActive = false;
		}
	}
}
