// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGComputeSourceFactory.h"

#include "Compute/PCGComputeSource.h"

UPCGComputeSourceFactory::UPCGComputeSourceFactory()
{
	SupportedClass = UPCGComputeSource::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UPCGComputeSourceFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UPCGComputeSource>(InParent, InClass, InName, Flags);
}
