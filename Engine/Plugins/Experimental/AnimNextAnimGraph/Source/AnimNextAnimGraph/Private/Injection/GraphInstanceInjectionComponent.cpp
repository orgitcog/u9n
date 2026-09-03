// Copyright Epic Games, Inc. All Rights Reserved.

#include "GraphInstanceInjectionComponent.h"
#include "Graph/AnimNextGraphInstance.h"

namespace UE::AnimNext
{

FGraphInstanceInjectionComponent::FGraphInstanceInjectionComponent(FAnimNextGraphInstance& InOwnerInstance)
	: FGraphInstanceComponent(InOwnerInstance)
	, InjectionInfo(InOwnerInstance)
{
}

}
