// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModuleInjectionDataInterfaceAdapter.h"

#include "AnimNextPool.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/ModuleGuard.h"

namespace UE::AnimNext
{

FModuleInjectionDataInterfaceAdapter::FModuleInjectionDataInterfaceAdapter(FAnimNextModuleInstance* InModuleInstance, FModuleHandle InOtherModuleHandle)
{
	if(InModuleInstance == nullptr || !InOtherModuleHandle.IsValid() || InModuleInstance->Pool == nullptr)
	{
		return;
	}

	const FAnimNextModuleInstance* FoundOtherModuleInstance = InModuleInstance->Pool->TryGet(InOtherModuleHandle);
	if(FoundOtherModuleInstance == nullptr)
	{
		return;
	}

	if(!ensureAlways(InModuleInstance->IsPrerequisite(*FoundOtherModuleInstance)))
	{
		return;
	}

	OtherModuleInstance = FoundOtherModuleInstance;
}

const UAnimNextDataInterface* FModuleInjectionDataInterfaceAdapter::GetDataInterface() const
{
	return OtherModuleInstance ? OtherModuleInstance->GetDataInterface() : nullptr;
}

uint8* FModuleInjectionDataInterfaceAdapter::GetMemoryForVariable(int32 InVariableIndex, FName InVariableName, const FProperty* InVariableProperty) const
{
	if(OtherModuleInstance == nullptr)
	{
		return nullptr;
	}
	return OtherModuleInstance->GetMemoryForVariable(InVariableIndex, InVariableName, InVariableProperty);
}

}
