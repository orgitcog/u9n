// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "DebugVisLocationProcessor.generated.h"

#define UE_API MASSGAMEPLAYDEBUG_API

class UMassDebugVisualizationComponent;
struct FSimDebugVisFragment;

UCLASS(MinimalAPI)
class UDebugVisLocationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UE_API UDebugVisLocationProcessor();
	
protected:
	UE_API virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	UE_API virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
	FMassEntityQuery AllLocationEntitiesQuery;
};

//----------------------------------------------------------------------//
// new one 
//----------------------------------------------------------------------//
//class UMassDebugger;

UCLASS(MinimalAPI)
class UMassProcessor_UpdateDebugVis : public UMassProcessor
{
	GENERATED_BODY()
public:
	UE_API UMassProcessor_UpdateDebugVis();
	UE_API virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	UE_API virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

protected:
	FMassEntityQuery EntityQuery;
};

#undef UE_API
