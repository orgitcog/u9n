// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassCrowdServerRepresentationProcessor.h"
#include "MassCrowdFragments.h"
#include "MassLODTypes.h"

UMassCrowdServerRepresentationProcessor::UMassCrowdServerRepresentationProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server;

	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LOD);
}

void UMassCrowdServerRepresentationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);
	EntityQuery.AddTagRequirement<FMassCrowdTag>(EMassFragmentPresence::All);
}
