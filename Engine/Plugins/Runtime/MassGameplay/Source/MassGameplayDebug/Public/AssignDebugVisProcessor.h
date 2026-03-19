// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassObserverProcessor.h"
#include "AssignDebugVisProcessor.generated.h"

#define UE_API MASSGAMEPLAYDEBUG_API


class UMassDebugVisualizationComponent;
struct FSimDebugVisFragment;

UCLASS(MinimalAPI)
class UAssignDebugVisProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()
public:
	UE_API UAssignDebugVisProcessor();
	UE_API virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	UE_API virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

protected:
	FMassEntityQuery EntityQuery;
};

#undef UE_API
