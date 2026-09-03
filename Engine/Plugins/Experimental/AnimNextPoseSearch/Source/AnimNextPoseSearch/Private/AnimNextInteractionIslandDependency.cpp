// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInteractionIslandDependency.h"

#include "Component/AnimNextComponent.h"
#include "Module/RigUnit_AnimNextModuleEvents.h"

namespace UE::PoseSearch
{

FAnimNextInteractionIslandDependency FAnimNextInteractionIslandDependency::ModularFeature;

bool FAnimNextInteractionIslandDependency::CanMakeDependency(const UObject* InIslandObject, const UObject* InObject) const
{
	return InObject->IsA<UAnimNextComponent>();
}
	
const FTickFunction* FAnimNextInteractionIslandDependency::FindTickFunction(UObject* InObject) const
{
	const UAnimNextComponent* AnimNextComponent = CastChecked<UAnimNextComponent>(InObject);
	return AnimNextComponent->FindTickFunction(FRigUnit_AnimNextPrePhysicsEvent::DefaultEventName);
}

void FAnimNextInteractionIslandDependency::AddPrerequisite(UObject* InIslandObject, FTickFunction& InIslandTickFunction, UObject* InObject) const
{
	UAnimNextComponent* AnimNextComponent = CastChecked<UAnimNextComponent>(InObject);
	AnimNextComponent->AddSubsequent(InIslandObject, InIslandTickFunction, FRigUnit_AnimNextPrePhysicsEvent::DefaultEventName);
}

void FAnimNextInteractionIslandDependency::AddSubsequent(UObject* InIslandObject, FTickFunction& InIslandTickFunction, UObject* InObject) const
{
	UAnimNextComponent* AnimNextComponent = CastChecked<UAnimNextComponent>(InObject);
	AnimNextComponent->AddPrerequisite(InIslandObject, InIslandTickFunction, FRigUnit_AnimNextPrePhysicsEvent::DefaultEventName);
}

void FAnimNextInteractionIslandDependency::RemovePrerequisite(UObject* InIslandObject, FTickFunction& InIslandTickFunction, UObject* InObject) const
{
	UAnimNextComponent* AnimNextComponent = CastChecked<UAnimNextComponent>(InObject);
	AnimNextComponent->RemoveSubsequent(InIslandObject, InIslandTickFunction, FRigUnit_AnimNextPrePhysicsEvent::DefaultEventName);
}

void FAnimNextInteractionIslandDependency::RemoveSubsequent(UObject* InIslandObject, FTickFunction& InIslandTickFunction, UObject* InObject) const
{
	UAnimNextComponent* AnimNextComponent = CastChecked<UAnimNextComponent>(InObject);
	AnimNextComponent->RemovePrerequisite(InIslandObject, InIslandTickFunction, FRigUnit_AnimNextPrePhysicsEvent::DefaultEventName);
}

}
