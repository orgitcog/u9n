// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compilation/AnimNextGetFunctionHeaderCompileContext.h"

FAnimNextGetFunctionHeaderCompileContext::FAnimNextGetFunctionHeaderCompileContext(FAnimNextRigVMAssetCompileContext& InCompilerContext)
	: CompilerContext(InCompilerContext)
{
}

const TArray<FAnimNextProgrammaticFunctionHeader>& FAnimNextGetFunctionHeaderCompileContext::GetFunctionHeaders() const
{
	return CompilerContext.FunctionHeaders;
}

TArray<FAnimNextProgrammaticFunctionHeader>& FAnimNextGetFunctionHeaderCompileContext::GetMutableFunctionHeaders()
{
	return CompilerContext.FunctionHeaders;
}
