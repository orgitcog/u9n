// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "StructUtils/PropertyBag.h"
#include "Variables/AnimNextVariableBinding.h"

struct FRigVMGraphFunctionArgument;

/**
 * Struct wrapping a graph variable. Includes default value.
 */
struct ANIMNEXTUNCOOKEDONLY_API FAnimNextProgrammaticVariable 
{

	/** Get arg AnimNext param type */
	FAnimNextParamType GetType() const;

	/** Set AnimNext param type, also sets propertybag to hold that type */
	bool SetType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true);

	/** Get VM variable name */
	FName GetVariableName() const;

	/** Set VM variable name */
	void SetVariableName(FName InName, bool bSetupUndoRedo = true);

	/** Set variable default value in propertybag */
	bool SetDefaultValue(TConstArrayView<uint8> InValue, bool bSetupUndoRedo = true);

	/** Set variable default value in propertybag via string */
	bool SetDefaultValueFromString(const FString& InDefaultValue, bool bSetupUndoRedo = true);

	/** Get inner propertybag storing values */
	const FInstancedPropertyBag& GetPropertyBag() const;

	/** Get inner propertybag storing values, mutable */
	FInstancedPropertyBag& GetMutablePropertyBag();

	/** Get default value via property */
	bool GetDefaultValue(const FProperty*& OutProperty, TConstArrayView<uint8>& OutValue) const;

	/** Access the memory for the internal value */
	const uint8* GetValuePtr() const;

public:

	/** Name of the variable */
	FName Name;

	/** The variable's type */
	FAnimNextParamType Type = FAnimNextParamType::GetType<bool>();

	/** Property bag holding the default value of the variable */
	FInstancedPropertyBag DefaultValue;

public: 

	/** Construct a parameter type from the passed in FRigVMTemplateArgumentType. */
	static FAnimNextProgrammaticVariable FromRigVMGraphFunctionArgument(const FRigVMGraphFunctionArgument& RigVMGraphFunctionArgument);
};

