// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compilation/AnimNextGetGraphCompileContext.h"

FAnimNextGetGraphCompileContext::FAnimNextGetGraphCompileContext(FAnimNextRigVMAssetCompileContext& InCompilerContext)
	: CompilerContext(InCompilerContext)
{
}

const TArray<FAnimNextProgrammaticFunctionHeader>& FAnimNextGetGraphCompileContext::GetFunctionHeaders() const
{
	return CompilerContext.FunctionHeaders;
}

const TArray<URigVMGraph*>& FAnimNextGetGraphCompileContext::GetProgrammaticGraphs() const
{
	return CompilerContext.ProgrammaticGraphs;
}

TArray<URigVMGraph*>& FAnimNextGetGraphCompileContext::GetMutableProgrammaticGraphs() 
{
	return CompilerContext.ProgrammaticGraphs;
}
