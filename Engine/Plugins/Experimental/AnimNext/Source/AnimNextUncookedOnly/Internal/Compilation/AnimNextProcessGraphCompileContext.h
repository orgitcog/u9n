// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compilation/AnimNextRigVMAssetCompileContext.h"

/**
 * Struct holding temporary compilation info during function header population
 */
struct ANIMNEXTUNCOOKEDONLY_API FAnimNextProcessGraphCompileContext
{
	FAnimNextProcessGraphCompileContext(FAnimNextRigVMAssetCompileContext& InCompilerContext);

public:

	const TArray<URigVMGraph*>& GetAllGraphs() const;
	TArray<URigVMGraph*>& GetMutableAllGraphs();

protected:

	FAnimNextRigVMAssetCompileContext& CompilerContext;
};

