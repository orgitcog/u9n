// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ComponentInterfaces.h"
#include "Components/ComponentInterfaceIterator.h"
#include "HAL/IConsoleManager.h"
#include "UnrealEngine.h"

TArray<FComponentInterfaceImplementation> IPrimitiveComponent::Implementers;

void IPrimitiveComponent::AddImplementer(const FComponentInterfaceImplementation& Implementer)
{	
	Implementers.Add(Implementer);
}

void IPrimitiveComponent::RemoveImplementer(const UClass* ImplementerClass)
{
	for (TArray<FComponentInterfaceImplementation>::TIterator ImplementerIt(Implementers); ImplementerIt; ++ImplementerIt)
	{
		if (ImplementerIt->Class == ImplementerClass)
		{
			ImplementerIt.RemoveCurrent();
		}
	}
}

TArray<FComponentInterfaceImplementation> IStaticMeshComponent::Implementers;

void IStaticMeshComponent::AddImplementer(const FComponentInterfaceImplementation& Implementer)
{
	Implementers.Add(Implementer);
}

void IStaticMeshComponent::RemoveImplementer(const UClass* ImplementerClass)
{
	for (TArray<FComponentInterfaceImplementation>::TIterator ImplementerIt(Implementers); ImplementerIt; ++ImplementerIt)
	{
		if (ImplementerIt->Class == ImplementerClass)
		{
			ImplementerIt.RemoveCurrent();
		}
	}
}
