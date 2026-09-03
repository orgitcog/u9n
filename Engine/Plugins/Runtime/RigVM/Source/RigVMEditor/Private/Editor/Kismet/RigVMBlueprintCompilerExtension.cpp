// Copyright Epic Games, Inc. All Rights Reserved.


#include "Editor/Kismet/RigVMBlueprintCompilerExtension.h"

class FKismetCompilerContext;

URigVMBlueprintCompilerExtension::URigVMBlueprintCompilerExtension(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
{
}

#if !WITH_RIGVMLEGACYEDITOR

void URigVMBlueprintCompilerExtension::BlueprintCompiled(const FKismetCompilerContext& CompilationContext, const FRigVMBlueprintCompiledData& Data)
{
	// common entry point in case we need to add logging, profiling, etc
	ProcessBlueprintCompiled(CompilationContext, Data);
}

#endif