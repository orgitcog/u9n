// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterface/AnimNextDataInterface.h"
#include "Param/ParamType.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "StructUtils/PropertyBag.h"
#include "RewindDebugger/AnimNextTrace.h"
#include "AnimNextDataInterfaceInstance.generated.h"

#define UE_API ANIMNEXT_API

class UAnimNextDataInterface;
struct FAnimNextModuleInjectionComponent;

namespace UE::AnimNext
{
	struct FInjectionInfo;
}

// Base struct for data interface-derived instances
USTRUCT()
struct FAnimNextDataInterfaceInstance
{
	GENERATED_BODY()
	
	UE_API FAnimNextDataInterfaceInstance();

	// Get the data interface asset that this instance represents
	const UAnimNextDataInterface* GetDataInterface() const
	{
		return DataInterface;
	}

	// Safely get the name of the data interface that this host provides
	FName GetDataInterfaceName() const
	{
		return DataInterface ? DataInterface->GetFName() : NAME_None;
	}

	// Get the property bag that holds external variables for this instance
	const FInstancedPropertyBag& GetVariables() const
	{
		return Variables;
	}

	// Get the RigVM extended execute context
	FRigVMExtendedExecuteContext& GetExtendedExecuteContext()
	{
		return ExtendedExecuteContext;
	}

	// Helper function used for bindings
	// Get the memory for the supplied variable, at the specified index
	// @param    InVariableIndex    The index into the data interface of the variable
	// @param    InVariableName     The name of the variable
	// @param    InVariableProperty The property of the variable
	UE_API uint8* GetMemoryForVariable(int32 InVariableIndex, FName InVariableName, const FProperty* InVariableProperty) const;

	// Get a variable's value given its name.
	// @param	InVariableName		The name of the variable to get the value of
	// @param	OutResult			Result that will be filled if no errors occur
	// @return see EPropertyBagResult
	template<typename ValueType>
	EPropertyBagResult GetVariable(FName InVariableName, ValueType& OutResult) const
	{
		return GetVariableInternal(InVariableName, FAnimNextParamType::GetType<ValueType>(), TArrayView<uint8>(reinterpret_cast<uint8*>(&OutResult), sizeof(ValueType)));
	}

	// Set a variable's value given its name.
	// @param	InVariableName		The name of the variable to set the value of
	// @param	OutResult			Result that will be filled if no errors occur
	// @return see EPropertyBagResult
	template<typename ValueType>
	EPropertyBagResult SetVariable(const FName InVariableName, const ValueType& InNewValue)
	{
		return SetVariableInternal(InVariableName, FAnimNextParamType::GetType<ValueType>(), TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InNewValue), sizeof(ValueType)));
	}

	// Get the instance (graph, module etc.) that owns/hosts us
	FAnimNextDataInterfaceInstance* GetHost() const { return HostInstance; }
	
#if ANIMNEXT_TRACE_ENABLED
	uint64 GetUniqueId() const
	{
		return UniqueId;
	}
#endif 

private:
	// Helper function for GetVariable
	UE_API EPropertyBagResult GetVariableInternal(FName InVariableName, const FAnimNextParamType& InType, TArrayView<uint8> OutResult) const;
	// Helper function for SetVariable
	UE_API EPropertyBagResult SetVariableInternal(const FName InVariableName, const FAnimNextParamType& InType, TConstArrayView<uint8> InNewValue);

protected:
	// Hard reference to the asset used to create this instance to ensure we can release it safely
	UPROPERTY(Transient)
	TObjectPtr<const UAnimNextDataInterface> DataInterface;

	// User variables used to operate the graph
	UPROPERTY(Transient)
	FInstancedPropertyBag Variables;

	// Extended execute context instance for this graph instance, we own it
	UPROPERTY(Transient)
	FRigVMExtendedExecuteContext ExtendedExecuteContext;

	// The instance (graph, module etc.) that owns/hosts us
	FAnimNextDataInterfaceInstance* HostInstance = nullptr;

	friend UE::AnimNext::FInjectionInfo;
	friend FAnimNextModuleInjectionComponent;
	
#if ANIMNEXT_TRACE_ENABLED
	uint64 UniqueId;
	UE_API volatile static int64 NextUniqueId;
#endif 
};

#undef UE_API
