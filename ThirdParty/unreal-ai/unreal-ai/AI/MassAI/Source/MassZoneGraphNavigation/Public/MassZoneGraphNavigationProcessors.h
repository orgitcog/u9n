// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassObserverProcessor.h"
#include "MassZoneGraphNavigationProcessors.generated.h"

#define UE_API MASSZONEGRAPHNAVIGATION_API

class UMassSignalSubsystem;


/**
 * Processor for initializing nearest location on ZoneGraph.
 */
UCLASS(MinimalAPI)
class UMassZoneGraphLocationInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()
	
public:
	UE_API UMassZoneGraphLocationInitializer();

protected:
	UE_API virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	UE_API virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** 
 * Processor for updating move target on ZoneGraph path.
 */
UCLASS(MinimalAPI)
class UMassZoneGraphPathFollowProcessor : public UMassProcessor
{
	GENERATED_BODY()

protected:
	UE_API UMassZoneGraphPathFollowProcessor();
	
	UE_API virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	UE_API virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	UE_API virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery_Conditional;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem = nullptr;
};

/** ZoneGraph lane cache boundary processor */
// @todo MassMovement: Make this signal based.
UCLASS(MinimalAPI)
class UMassZoneGraphLaneCacheBoundaryProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UE_API UMassZoneGraphLaneCacheBoundaryProcessor();

protected:
	UE_API virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	UE_API virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	UE_API virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	TWeakObjectPtr<UWorld> WeakWorld;
	FMassEntityQuery EntityQuery;
};

#undef UE_API
