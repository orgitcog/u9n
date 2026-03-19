// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextModuleAnimGraphComponent.h"

#include "Graph/AnimNextGraphInstance.h"
#include "Graph/AnimNextAnimationGraph.h"

TWeakPtr<FAnimNextGraphInstance> FAnimNextModuleAnimGraphComponent::AllocateInstance(const UAnimNextAnimationGraph* InAnimationGraph, FAnimNextGraphInstance* InParentInstance, FName InEntryPoint)
{
	check(InAnimationGraph);

	TSharedPtr<FAnimNextGraphInstance> NewInstance = InAnimationGraph->AllocateInstance(&GetModuleInstance(), nullptr, InParentInstance, InEntryPoint);
	if(!NewInstance.IsValid())
	{
		return TWeakPtr<FAnimNextGraphInstance>();
	}

	GraphInstances.Add(NewInstance);
	return NewInstance;
}

void FAnimNextModuleAnimGraphComponent::ReleaseInstance(TWeakPtr<FAnimNextGraphInstance> InWeakInstance)
{
	TSharedPtr<FAnimNextGraphInstance> PinnedInstance = InWeakInstance.Pin();
	if(PinnedInstance.IsValid())
	{
		check(GraphInstances.Contains(PinnedInstance));			// Should not be releasing this instance if it is not owned by this module
		GraphInstances.Remove(PinnedInstance);
		check(PinnedInstance.GetSharedReferenceCount() == 1);	// This should be the final reference, all others should be weak
	}
}

void FAnimNextModuleAnimGraphComponent::AddStructReferencedObjects(class FReferenceCollector& Collector)
{
	for (const TSharedPtr<FAnimNextGraphInstance>& GraphInstance : GraphInstances)
	{
		GraphInstance->AddStructReferencedObjects(Collector);
	}
}

