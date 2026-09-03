// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterface/AnimNextDataInterfaceInstance.h"
#include "AnimNextModuleContextData.generated.h"

struct FAnimNextModuleInstance;

USTRUCT()
struct FAnimNextModuleContextData
{
	GENERATED_BODY()

	FAnimNextModuleContextData() = default;

	ANIMNEXT_API explicit FAnimNextModuleContextData(FAnimNextModuleInstance* InModuleInstance);

	ANIMNEXT_API FAnimNextModuleContextData(FAnimNextModuleInstance* InModuleInstance, const FAnimNextDataInterfaceInstance* InDataInterfaceInstance);

	// Get the object that the module instance is bound to, if any
	ANIMNEXT_API UObject* GetObject() const;

	FAnimNextModuleInstance& GetModuleInstance() const
	{
		check(ModuleInstance != nullptr);
		return *ModuleInstance;
	}

	const FAnimNextDataInterfaceInstance& GetDataInterfaceInstance() const
	{
		check(DataInterfaceInstance != nullptr);
		return *DataInterfaceInstance;
	}

private:
	// Module instance that is currently executing.
	FAnimNextModuleInstance* ModuleInstance = nullptr;

	// Data interface that is currently executing. Can be the same as ModuleInstance
	const FAnimNextDataInterfaceInstance* DataInterfaceInstance = nullptr;

	friend class UAnimNextModule;
	friend struct FAnimNextExecuteContext;
};
