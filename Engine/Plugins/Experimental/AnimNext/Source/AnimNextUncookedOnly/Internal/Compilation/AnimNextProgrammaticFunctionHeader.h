// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "StructUtils/PropertyBag.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"

/**
 * Struct wrapping a RigVM Function header. Also contains some metadata used to drive compilation.
 */
struct ANIMNEXTUNCOOKEDONLY_API FAnimNextProgrammaticFunctionHeader
{
	/** RigVM Function Header we are wrapping */
	FRigVMGraphFunctionHeader Wrapped;

	/** 
	 * True instructs compiler that we should generate & link programmatic variables for this function's params
	 * 
	 * Used by those who cannot set params by linking into the function node during graph compile time (Ex: StateTree).
	 */
	bool bGenerateParamVariables = false;

	/** 
	 * True instructs compiler that we should generate & link programmatic variables for this function's returns  
	 * 
	 * Used by those who cannot get returns by linking into the function node during graph compile time (Ex: StateTree).
	 */
	bool bGenerateReturnVariables = false;
};

