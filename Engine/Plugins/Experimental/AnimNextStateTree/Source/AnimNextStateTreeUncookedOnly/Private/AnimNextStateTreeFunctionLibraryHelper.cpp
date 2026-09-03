// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextStateTreeFunctionLibraryHelper.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "UncookedOnlyUtils.h"

const TArray<FName> UAnimNextStateTreeFunctionLibraryHelper::GetExposedAnimNextFunctionNames()
{
	// Can't cache here since the list of functions is dynamic.
	TArray<FName> Result;

	TMap<FAssetData, FRigVMGraphFunctionHeaderArray> FunctionExports;
	UE::AnimNext::UncookedOnly::FUtils::GetExportedFunctionsFromAssetRegistry(UE::AnimNext::AnimNextPublicGraphFunctionsExportsRegistryTag, FunctionExports);
	UE::AnimNext::UncookedOnly::FUtils::GetExportedFunctionsFromAssetRegistry(UE::AnimNext::ControlRigAssetPublicGraphFunctionsExportsRegistryTag, FunctionExports);
	
	for (const TPair<FAssetData, FRigVMGraphFunctionHeaderArray>& Export : FunctionExports.Array())
	{
		for (const FRigVMGraphFunctionHeader& FunctionHeader : Export.Value.Headers)
		{
			Result.Add(FunctionHeader.Name);
		}
	}

	return Result;
}