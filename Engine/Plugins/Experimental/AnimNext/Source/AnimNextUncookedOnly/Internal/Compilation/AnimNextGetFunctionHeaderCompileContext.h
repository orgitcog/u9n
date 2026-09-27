// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compilation/AnimNextRigVMAssetCompileContext.h"

/**
 * Struct holding temporary compilation info during function header population 
 */
struct ANIMNEXTUNCOOKEDONLY_API FAnimNextGetFunctionHeaderCompileContext
{
	FAnimNextGetFunctionHeaderCompileContext(FAnimNextRigVMAssetCompileContext& InCompilerContext);

public:
	
	const TArray<FAnimNextProgrammaticFunctionHeader>& GetFunctionHeaders() const;
	TArray<FAnimNextProgrammaticFunctionHeader>& GetMutableFunctionHeaders();

protected:

	FAnimNextRigVMAssetCompileContext& CompilerContext;
};

