// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuroEntity.h"

ANeuroEntity::ANeuroEntity()
	: EntityType(ENeuroType::Standard)
	, CognitivePower(75.0f)
	, NeuralEfficiency(0.85f)
	, ProcessingSpeed(1.0f)
	, CurrentState(TEXT("Idle"))
	, StateTimer(0.0f)
{
	PrimaryActorTick.bCanEverTick = true;

	// Create Neuro network
	NeuroNetwork = CreateDefaultSubobject<UEchoStateNetwork>(TEXT("NeuroNetwork"));
}

void ANeuroEntity::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeNeuro(EntityType);
}

void ANeuroEntity::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateNeuroState(DeltaTime);
}

void ANeuroEntity::InitializeNeuro(ENeuroType Type)
{
	EntityType = Type;
	
	// Configure based on type
	switch (EntityType)
	{
		case ENeuroType::Standard:
			CognitivePower = 75.0f;
			NeuralEfficiency = 0.85f;
			ProcessingSpeed = 1.0f;
			if (NeuroNetwork)
			{
				NeuroNetwork->InitializeNetwork(4, 80);
			}
			break;

		case ENeuroType::Advanced:
			CognitivePower = 90.0f;
			NeuralEfficiency = 0.95f;
			ProcessingSpeed = 1.5f;
			if (NeuroNetwork)
			{
				NeuroNetwork->InitializeNetwork(6, 120);
			}
			break;

		case ENeuroType::Fusion:
			CognitivePower = 100.0f;
			NeuralEfficiency = 1.0f;
			ProcessingSpeed = 2.0f;
			if (NeuroNetwork)
			{
				NeuroNetwork->InitializeNetwork(8, 150);
			}
			break;
	}

	CurrentState = TEXT("Active");
	
	UE_LOG(LogTemp, Log, TEXT("NeuroEntity: Initialized as type %d with power %.1f"), 
		static_cast<int32>(EntityType), CognitivePower);
}

TArray<float> ANeuroEntity::ProcessNeuralInput(const TArray<float>& Input)
{
	if (!NeuroNetwork)
	{
		return TArray<float>();
	}

	// Process through network
	TArray<float> Output = NeuroNetwork->ProcessInput(Input);

	// Apply neural efficiency
	for (float& Value : Output)
	{
		Value *= NeuralEfficiency;
	}

	// Apply processing speed
	for (float& Value : Output)
	{
		Value *= ProcessingSpeed;
	}

	return Output;
}

void ANeuroEntity::UpdateNeuroState(float DeltaTime)
{
	StateTimer += DeltaTime;

	if (NeuroNetwork)
	{
		NeuroNetwork->UpdateNetworkState(DeltaTime * ProcessingSpeed);
	}

	// Update cognitive power over time
	float PowerFluctuation = FMath::Sin(StateTimer * 0.5f) * 5.0f;
	CognitivePower = FMath::Clamp(CognitivePower + PowerFluctuation * DeltaTime, 50.0f, 100.0f);
}

float ANeuroEntity::GetCombatPower() const
{
	return CognitivePower * NeuralEfficiency * ProcessingSpeed;
}
