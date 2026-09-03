// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMTrait_ModuleEventDependency_ActorComponentPrimaryTickFunction.h"

#if WITH_EDITOR
FString FRigVMTrait_ModuleEventDependency_ActorComponentPrimaryTickFunction::GetDisplayName() const
{
	return StaticStruct()->GetDisplayNameText().ToString();
}
#endif

void FRigVMTrait_ModuleEventDependency_ActorComponentPrimaryTickFunction::OnAddDependency(const UE::AnimNext::FModuleDependencyContext& InContext) const
{
	UActorComponent* AnimNextComponent = Cast<UActorComponent>(InContext.Object);
	if(AnimNextComponent == nullptr)
	{
		return;
	}

	if(Component == nullptr)
	{
		return;
	}

	if(Ordering == EAnimNextModuleEventDependencyOrdering::Before)
	{
		InContext.TickFunction.AddPrerequisite(Component, Component->PrimaryComponentTick);
	}
	else
	{
		Component->PrimaryComponentTick.AddPrerequisite(AnimNextComponent, InContext.TickFunction);
	}
}

void FRigVMTrait_ModuleEventDependency_ActorComponentPrimaryTickFunction::OnRemoveDependency(const UE::AnimNext::FModuleDependencyContext& InContext) const
{
	UActorComponent* AnimNextComponent = Cast<UActorComponent>(InContext.Object);
	if(AnimNextComponent == nullptr)
	{
		return;
	}
	
	if(Component == nullptr)
	{
		return;
	}

	if(Ordering == EAnimNextModuleEventDependencyOrdering::Before)
	{
		InContext.TickFunction.RemovePrerequisite(Component, Component->PrimaryComponentTick);
	}
	else
	{
		Component->PrimaryComponentTick.RemovePrerequisite(AnimNextComponent, InContext.TickFunction);
	}
}