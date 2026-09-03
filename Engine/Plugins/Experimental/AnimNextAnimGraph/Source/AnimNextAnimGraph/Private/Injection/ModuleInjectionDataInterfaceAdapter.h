// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterface/AnimNextDataInterfaceHost.h"
#include "Module/ModuleHandle.h"
#include "Module/ModuleGuard.h"

namespace UE::AnimNext
{

// Adapter allowing external systems to implement a data interface
// Used at instantiation time (and in editor, re-binding after compilation time) to bind directly to a host's memory for an interfaces variables
struct FModuleInjectionDataInterfaceAdapter : public IDataInterfaceHost
{
public:
	FModuleInjectionDataInterfaceAdapter() = default;

	FModuleInjectionDataInterfaceAdapter(FAnimNextModuleInstance* InModuleInstance, FModuleHandle InOtherModuleHandle);

private:
	// IDataInterfaceHost interface
	virtual const UAnimNextDataInterface* GetDataInterface() const override;
	virtual uint8* GetMemoryForVariable(int32 InVariableIndex, FName InVariableName, const FProperty* InVariableProperty) const override;

	const FAnimNextModuleInstance* OtherModuleInstance = nullptr;
};

}
