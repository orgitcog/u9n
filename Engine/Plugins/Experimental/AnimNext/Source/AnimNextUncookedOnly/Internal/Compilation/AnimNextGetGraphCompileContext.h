// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compilation/AnimNextRigVMAssetCompileContext.h"

/**
 * Struct holding temporary compilation info during function header population
 */
struct ANIMNEXTUNCOOKEDONLY_API FAnimNextGetGraphCompileContext
{
	FAnimNextGetGraphCompileContext(FAnimNextRigVMAssetCompileContext& InCompilerContext);

public:

	const TArray<FAnimNextProgrammaticFunctionHeader>& GetFunctionHeaders() const;

	const TArray<URigVMGraph*>& GetProgrammaticGraphs() const;
	TArray<URigVMGraph*>& GetMutableProgrammaticGraphs();

protected:

	FAnimNextRigVMAssetCompileContext& CompilerContext;
};

