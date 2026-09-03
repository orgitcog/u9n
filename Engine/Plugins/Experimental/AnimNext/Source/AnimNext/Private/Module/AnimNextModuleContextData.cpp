// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModuleContextData.h"
#include "Module/AnimNextModuleInstance.h"

FAnimNextModuleContextData::FAnimNextModuleContextData(FAnimNextModuleInstance* InModuleInstance)
	: ModuleInstance(InModuleInstance)
	, DataInterfaceInstance(InModuleInstance)
{
}

FAnimNextModuleContextData::FAnimNextModuleContextData(FAnimNextModuleInstance* InModuleInstance, const FAnimNextDataInterfaceInstance* InDataInterfaceInstance)
	: ModuleInstance(InModuleInstance)
	, DataInterfaceInstance(InDataInterfaceInstance)
{
}

UObject* FAnimNextModuleContextData::GetObject() const
{
	return ModuleInstance ? ModuleInstance->GetObject() : nullptr;
}