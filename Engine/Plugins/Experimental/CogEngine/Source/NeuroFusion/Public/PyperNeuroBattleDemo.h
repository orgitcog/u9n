// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PyperActor.h"
#include "NeuroEntity.h"
#include "SuperhotFusionBattle.h"
#include "PyperNeuroBattleDemo.generated.h"

/**
 * Pyper vs Neuro Battle Demo
 * Example actor demonstrating the Superhot Fusion Battle system
 */
UCLASS()
class NEUROFUSION_API APyperNeuroBattleDemo : public AActor
{
	GENERATED_BODY()
	
public:	
	APyperNeuroBattleDemo();

	/** Pyper actor reference */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Demo")
	APyperActor* Pyper;

	/** Neuro entity reference */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Demo")
	ANeuroEntity* Neuro;

	/** Battle system reference */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Demo")
	ASuperhotFusionBattle* BattleSystem;

	/** Auto-start battle on begin play */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Demo")
	bool bAutoStartBattle;

	/** Pyper model path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Demo")
	FString PyperModelPath;

	/** Neuro type to spawn */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Demo")
	ENeuroType NeuroTypeToSpawn;

	/** Initialize demo */
	UFUNCTION(BlueprintCallable, Category = "Demo")
	void InitializeDemo();

	/** Start the battle */
	UFUNCTION(BlueprintCallable, Category = "Demo")
	void StartDemo();

	/** Stop the battle */
	UFUNCTION(BlueprintCallable, Category = "Demo")
	void StopDemo();

	/** Get current battle status */
	UFUNCTION(BlueprintCallable, Category = "Demo")
	FString GetBattleStatus() const;

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

private:
	/** Spawn and setup entities */
	void SpawnEntities();

	/** Update demo state */
	void UpdateDemo(float DeltaTime);

	bool bDemoActive;
};
