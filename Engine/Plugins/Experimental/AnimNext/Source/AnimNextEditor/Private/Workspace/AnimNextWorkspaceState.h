// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextWorkspaceState.generated.h"

// Custom persistent workspace state 
USTRUCT()
struct FAnimNextWorkspaceState
{
	GENERATED_BODY()

	// Whether to automatically compile RigVM files after every edit
	UPROPERTY()
	bool bAutoCompile = true;

	// When manually compiling, whether to compile the current file or the whole workspace
	UPROPERTY()
	bool bCompileWholeWorkspace = true;

	// When manually compiling, whether to compile only dirty files. Files that are not dirty will get skipped.
	UPROPERTY()
	bool bCompileDirtyFiles = true;
};