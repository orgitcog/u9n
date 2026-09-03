// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMTrait_ModuleEventDependency_ActorComponentClassPrimaryTickFunction.h"

#if WITH_EDITOR
FString FRigVMTrait_ModuleEventDependency_ActorComponentClassPrimaryTickFunction::GetDisplayName() const
{
	return StaticStruct()->GetDisplayNameText().ToString();
}
#endif

void FRigVMTrait_ModuleEventDependency_ActorComponentClassPrimaryTickFunction::OnAddDependency(const UE::AnimNext::FModuleDependencyContext& InContext) const
{
	UActorComponent* AnimNextComponent = Cast<UActorComponent>(InContext.Object);
	if(AnimNextComponent == nullptr)
	{
		return;
	}

	AActor* Actor = AnimNextComponent->GetOwner();
	if(Actor == nullptr)
	{
		return;
	}

	UActorComponent* TargetComponent = Actor->GetComponentByClass(ComponentClass);
	if(TargetComponent == nullptr)
	{
		return;
	}

	if(Ordering == EAnimNextModuleEventDependencyOrdering::Before)
	{
		InContext.TickFunction.AddPrerequisite(TargetComponent, TargetComponent->PrimaryComponentTick);
	}
	else
	{
		TargetComponent->PrimaryComponentTick.AddPrerequisite(AnimNextComponent, InContext.TickFunction);
	}
}

void FRigVMTrait_ModuleEventDependency_ActorComponentClassPrimaryTickFunction::OnRemoveDependency(const UE::AnimNext::FModuleDependencyContext& InContext) const
{
	UActorComponent* AnimNextComponent = Cast<UActorComponent>(InContext.Object);
	if(AnimNextComponent == nullptr)
	{
		return;
	}

	AActor* Actor = AnimNextComponent->GetOwner();
	if(Actor == nullptr)
	{
		return;
	}

	UActorComponent* TargetComponent = Actor->GetComponentByClass(ComponentClass);
	if(TargetComponent == nullptr)
	{
		return;
	}

	if(Ordering == EAnimNextModuleEventDependencyOrdering::Before)
	{
		InContext.TickFunction.RemovePrerequisite(TargetComponent, TargetComponent->PrimaryComponentTick);
	}
	else
	{
		TargetComponent->PrimaryComponentTick.RemovePrerequisite(AnimNextComponent, InContext.TickFunction);
	}
}