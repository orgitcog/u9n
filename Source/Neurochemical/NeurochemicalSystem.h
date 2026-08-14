#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoPulseResonanceChamber.h"
#include "CognitiveEndorphinJelly.h"
#include "EchoPyperCuriosityModule.h"
#include "NeuroChaosIntensityController.h"
#include "DegradationRecoverySystem.h"
#include "AbundanceMonitor.h"
#include "ResourceTracker.h"
#include "ScarcityDetector.h"
#include "HomeostasisRegulator.h"
#include "NeurochemicalSystem.generated.h"

UCLASS()
class ANeurochemicalSystem : public AActor
{
	GENERATED_BODY()

public:
	ANeurochemicalSystem();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	UEchoPulseResonanceChamber* ResonanceChamber;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	UCognitiveEndorphinJelly* EndorphinJelly;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	UEchoPyperCuriosityModule* CuriosityModule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	UNeuroChaosIntensityController* ChaosController;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	UDegradationRecoverySystem* RecoverySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	UAbundanceMonitor* AbundanceMonitor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	UResourceTracker* ResourceTracker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	UScarcityDetector* ScarcityDetector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	UHomeostasisRegulator* HomeostasisRegulator;

	UFUNCTION()
	void HandleCuriosityTrigger();

	UFUNCTION()
	void HandleJellySpasm();

	UFUNCTION()
	void HandleRecovery();

	UFUNCTION()
	void HandleScarcity(FString ResourceName);
};
