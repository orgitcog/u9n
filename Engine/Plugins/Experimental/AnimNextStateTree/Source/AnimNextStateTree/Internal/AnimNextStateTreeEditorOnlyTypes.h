// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Compilation/AnimNextGetFunctionHeaderCompileContext.h"

class UAnimNextDataInterface_EditorData;
class URigVMGraph;

struct FRigVMCompileSettings;
struct FRigVMClient;

/**
 * Helper struct to contain info needed to compile RigVM Variables
 */
struct ANIMNEXTSTATETREE_API FAnimNextStateTreeProgrammaticFunctionHeaderParams
{
	FAnimNextStateTreeProgrammaticFunctionHeaderParams(UAnimNextDataInterface_EditorData* InEditorData, const FRigVMCompileSettings& InSettings, FRigVMClient& InRigVMClient, FAnimNextGetFunctionHeaderCompileContext& InOutCompileContext)
		: EditorData(InEditorData)
		, Settings(InSettings)
		, RigVMClient(InRigVMClient)
		, OutCompileContext(InOutCompileContext)
	{
	}

	UAnimNextDataInterface_EditorData* EditorData;
	const FRigVMCompileSettings& Settings;
	FRigVMClient& RigVMClient;
	FAnimNextGetFunctionHeaderCompileContext& OutCompileContext;
};

#endif // WITH_EDITOR