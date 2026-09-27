// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoStateNetwork.h"
#include "NeuroEntity.generated.h"

/**
 * Neuro Entity Type
 */
UENUM(BlueprintType)
enum class ENeuroType : uint8
{
	Standard UMETA(DisplayName = "Standard Neuro"),
	Advanced UMETA(DisplayName = "Advanced Neuro"),
	Fusion UMETA(DisplayName = "Fusion Neuro")
};

/**
 * Neuro Entity - Competitor to Pyper in cognitive battles
 * Alternative AI entity with different cognitive architecture
 */
UCLASS()
class NEUROFUSION_API ANeuroEntity : public AActor
{
	GENERATED_BODY()
	
public:	
	ANeuroEntity();

	/** Echo State Network for cognition */
	UPROPERTY(BlueprintReadOnly, Category = "Neuro|Cognition")
	UEchoStateNetwork* NeuroNetwork;

	/** Neuro type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Neuro")
	ENeuroType EntityType;

	/** Cognitive power level */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Neuro")
	float CognitivePower;

	/** Neural efficiency */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Neuro")
	float NeuralEfficiency;

	/** Processing speed multiplier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Neuro")
	float ProcessingSpeed;

	/** Current state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Neuro")
	FString CurrentState;

	/** Initialize Neuro entity */
	UFUNCTION(BlueprintCallable, Category = "Neuro")
	void InitializeNeuro(ENeuroType Type);

	/** Process neural input */
	UFUNCTION(BlueprintCallable, Category = "Neuro")
	TArray<float> ProcessNeuralInput(const TArray<float>& Input);

	/** Update Neuro state */
	UFUNCTION(BlueprintCallable, Category = "Neuro")
	void UpdateNeuroState(float DeltaTime);

	/** Get combat power */
	UFUNCTION(BlueprintCallable, Category = "Neuro")
	float GetCombatPower() const;

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

private:
	float StateTimer;
};
