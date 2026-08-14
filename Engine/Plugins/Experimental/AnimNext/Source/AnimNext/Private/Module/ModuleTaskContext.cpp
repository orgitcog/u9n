// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/ModuleTaskContext.h"

#include "Module/AnimNextModuleInstance.h"
#include "Module/ModuleGuard.h"

namespace UE::AnimNext
{

FModuleTaskContext::FModuleTaskContext(FAnimNextModuleInstance& InModuleInstance)
	: ModuleInstance(&InModuleInstance)
{
}

void FModuleTaskContext::QueueInputTraitEvent(FAnimNextTraitEventPtr Event) const
{
	FModuleWriteGuard Guard(ModuleInstance);

	ModuleInstance->QueueInputTraitEvent(MoveTemp(Event));
}

void FModuleTaskContext::TryAccessComponent(UScriptStruct* InComponentType, TFunctionRef<void(FAnimNextModuleInstanceComponent&)> InFunction) const
{
	FModuleWriteGuard Guard(ModuleInstance);

	FName ComponentName = InComponentType->GetFName();
	uint32 ComponentNameHash = GetTypeHash(ComponentName);
	FAnimNextModuleInstanceComponent* Component = ModuleInstance->TryGetComponent(ComponentNameHash, ComponentName);
	if(Component == nullptr)
	{
		return;
	}

	InFunction(*Component);
}

FAnimNextModuleInstance* const FModuleTaskContext::GetModuleInstance() const
{
	return ModuleInstance;
}

}
