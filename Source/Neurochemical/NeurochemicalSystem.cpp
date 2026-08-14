#include "NeurochemicalSystem.h"

ANeurochemicalSystem::ANeurochemicalSystem()
{
	PrimaryActorTick.bCanEverTick = true;

	ResonanceChamber = CreateDefaultSubobject<UEchoPulseResonanceChamber>(TEXT("ResonanceChamber"));
	EndorphinJelly = CreateDefaultSubobject<UCognitiveEndorphinJelly>(TEXT("EndorphinJelly"));
	CuriosityModule = CreateDefaultSubobject<UEchoPyperCuriosityModule>(TEXT("CuriosityModule"));
	ChaosController = CreateDefaultSubobject<UNeuroChaosIntensityController>(TEXT("ChaosController"));
	RecoverySystem = CreateDefaultSubobject<UDegradationRecoverySystem>(TEXT("RecoverySystem"));

	AbundanceMonitor = CreateDefaultSubobject<UAbundanceMonitor>(TEXT("AbundanceMonitor"));
	ResourceTracker = CreateDefaultSubobject<UResourceTracker>(TEXT("ResourceTracker"));
	ScarcityDetector = CreateDefaultSubobject<UScarcityDetector>(TEXT("ScarcityDetector"));
	HomeostasisRegulator = CreateDefaultSubobject<UHomeostasisRegulator>(TEXT("HomeostasisRegulator"));
}

void ANeurochemicalSystem::BeginPlay()
{
	Super::BeginPlay();

	CuriosityModule->OnTrigger.AddDynamic(this, &ANeurochemicalSystem::HandleCuriosityTrigger);
	EndorphinJelly->OnSpasm.AddDynamic(this, &ANeurochemicalSystem::HandleJellySpasm);
	RecoverySystem->OnRecovery.AddDynamic(this, &ANeurochemicalSystem::HandleRecovery);
	ScarcityDetector->OnScarcityDetected.AddDynamic(this, &ANeurochemicalSystem::HandleScarcity);
}

void ANeurochemicalSystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update serotonin system
	ResourceTracker->UpdateResource(TEXT("Memory"), 0.8f); // Dummy value
	ResourceTracker->UpdateResource(TEXT("CPU"), 0.6f); // Dummy value
	AbundanceMonitor->Update(ResourceTracker->GetResourceLevels());
	ScarcityDetector->CheckForScarcity(ResourceTracker);

	// Regulate dopamine system
	HomeostasisRegulator->Regulate(CuriosityModule, AbundanceMonitor->GetAbundanceState());

	CuriosityModule->ChaosFactor = ChaosController->ChaosLevel;
	CuriosityModule->Update(DeltaTime);

	RecoverySystem->Monitor(ChaosController->ChaosLevel);
}

void ANeurochemicalSystem::HandleCuriosityTrigger()
{
	ResonanceChamber->IntensityFactor = 1.0f + ChaosController->ChaosLevel * 5.0f;
	ResonanceChamber->UpdateResonance(10.0f);
	EndorphinJelly->Accumulate(ResonanceChamber->ResonanceLevel);
}

void ANeurochemicalSystem::HandleJellySpasm()
{
	// This is where the "strange behavior" would be triggered.
	// For now, we'll just log it.
	UE_LOG(LogTemp, Warning, TEXT("Jelly Spasm! Echofluids everywhere!"));
}

void ANeurochemicalSystem::HandleScarcity(FString ResourceName)
{
	HomeostasisRegulator->RespondToScarcity(ResourceName);
}

void ANeurochemicalSystem::HandleRecovery()
{
	ResonanceChamber->Reset();
	EndorphinJelly->Reset();
	ChaosController->SetChaosLevel(0.0f);
	UE_LOG(LogTemp, Warning, TEXT("System recovering..."));
}
