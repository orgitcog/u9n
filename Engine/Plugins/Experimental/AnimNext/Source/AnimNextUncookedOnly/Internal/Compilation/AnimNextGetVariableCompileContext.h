// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compilation/AnimNextRigVMAssetCompileContext.h"

/**
 * Struct holding temporary compilation info during function header population
 */
struct ANIMNEXTUNCOOKEDONLY_API FAnimNextGetVariableCompileContext
{
	FAnimNextGetVariableCompileContext(FAnimNextRigVMAssetCompileContext& InCompilerContext);

public:

	const TArray<FAnimNextProgrammaticFunctionHeader>& GetFunctionHeaders() const;

	const TArray<FAnimNextProgrammaticVariable>& GetProgrammaticVariables() const;
	TArray<FAnimNextProgrammaticVariable>& GetMutableProgrammaticVariables();

protected:

	FAnimNextRigVMAssetCompileContext& CompilerContext;
};

