// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SpatialReadinessAPI.h"
#include "SpatialReadinessSimCallback.h"
#include "SpatialReadinessSubsystem.generated.h"

UCLASS(MinimalAPI)
class USpatialReadiness : public UWorldSubsystem
{
	GENERATED_BODY()
	using This = USpatialReadiness;

public:
	USpatialReadiness();
	USpatialReadiness(FVTableHelper& Helper);

	// Add a volume which can be marked read/unready
	SPATIALREADINESS_API FSpatialReadinessVolume AddReadinessVolume(const FBox& Bounds, const FString& Description);

	// Check to see if a volume is ready
	//
	// If bAllDescriptions = false, OutDescriptions will contain only the description of the
	// first unready volume that we hit. If true, it will contain descriptions from all
	// overlapping volumes.
	SPATIALREADINESS_API bool QueryReadiness(const FBox& Bounds, TArray<FString>& OutDescriptions, bool bAllDescriptions = false) const;

	/* begin: UWorldSubsystem */
protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	/* end: UWorldSubsystem */

private:

	// The spatial readiness API object which manages giving out volume "handles"
	FSpatialReadinessAPI SpatialReadiness;

	// The functions to bind to the spatial readiness API.
	int32 AddUnreadyVolume(const FBox& Bounds, const FString& Description);
	void RemoveUnreadyVolume(int32 UnreadyVolumeIndex);

	// Create and destroy the physics sim callback
	bool CreateSimCallback();
	bool DestroySimCallback();

	// Getters
	FPhysScene_Chaos* GetScene();
	Chaos::FPhysicsSolver* GetSolver();

	FSpatialReadinessSimCallback* SimCallback = nullptr;
};
