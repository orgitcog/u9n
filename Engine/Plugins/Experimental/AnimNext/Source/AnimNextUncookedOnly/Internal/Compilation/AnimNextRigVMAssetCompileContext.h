// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "StructUtils/PropertyBag.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "Compilation/AnimNextProgrammaticFunctionHeader.h"
#include "Variables/AnimNextProgrammaticVariable.h"

class URigVMGraph;

/**
 * Struct holding temporary compilation info for a single RigVM asset
 * 
 * Note: Sub compile contexts give limited access. But feel free to expand
 * the access of any particular sub context as needed.
 */
struct ANIMNEXTUNCOOKEDONLY_API FAnimNextRigVMAssetCompileContext
{
private:
	friend class UAnimNextRigVMAssetEditorData;
	friend struct FAnimNextGetFunctionHeaderCompileContext;
	friend struct FAnimNextGetVariableCompileContext;
	friend struct FAnimNextGetGraphCompileContext;
	friend struct FAnimNextProcessGraphCompileContext;

protected:

	/** Function Headers we should generate variables / graphs for */
	TArray<FAnimNextProgrammaticFunctionHeader> FunctionHeaders;

	/** Programmtic variables generated during this compile */
	TArray<FAnimNextProgrammaticVariable> ProgrammaticVariables;

	/** Programmtic graphs generated during this compile */
	TArray<URigVMGraph*> ProgrammaticGraphs;

	/** All graphs compiled by this RigVM Asset */
	TArray<URigVMGraph*> AllGraphs;
};

