// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processors/MassEnvQueryProcessorBase.h"
#include "Engine/World.h"
#include "MassEQSSubsystem.h"

void UMassEnvQueryProcessorBase::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	if (CorrespondingRequestClass)
	{
		UWorld* World = Owner.GetWorld();
		check(World);
		UMassEQSSubsystem* MassEQSSubsystem = World->GetSubsystem<UMassEQSSubsystem>();
		check(MassEQSSubsystem)
	
		CachedRequestQueryIndex = MassEQSSubsystem->GetRequestQueueIndex(CorrespondingRequestClass);
	}
}