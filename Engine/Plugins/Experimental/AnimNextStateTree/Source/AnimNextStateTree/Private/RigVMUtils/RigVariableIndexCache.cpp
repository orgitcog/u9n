// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMUtils/RigVariableIndexCache.h"

#include "AnimNextExecuteContext.h"
#include "Graph/AnimNextGraphContextData.h"
#include "Graph/AnimNextGraphInstance.h"
#include "StructUtils/PropertyBag.h"
#include "TraitCore/ExecutionContext.h"

FString FRigVariableIndexCache::MakeFunctionWrapperVariableName(FName InFunctionName, FName InVariableName)
{
	// We assume the function name is enough for variable name uniqueness in this graph (We don't yet desire global uniqueness).
	return TEXT("__InternalVar_") + InFunctionName.ToString() + "_" + InVariableName.ToString();
}

FString FRigVariableIndexCache::MakeFunctionWrapperEventName(FName InFunctionName)
{
	return TEXT("__InternalCall_") + InFunctionName.ToString();
}

bool FRigVariableIndexCache::IsIndexCacheInitialized() const
{
	return bInitialized;
}

bool FRigVariableIndexCache::TryPopulateIndexCache(const FRigVMMemoryStorageStruct& ParamData, const UAnimNextDataInterface* InDataInterface, const FName InFunctionName, const FName InResultName)
{
	if (bInitialized)
	{
		return true;
	}

	ArgumentIndexes.Reserve(ParamData.Num());
	TArray<FRigVMExternalVariable> ExternalVariables = InDataInterface->GetExternalVariables();
	for (const FPropertyBagPropertyDesc& Desc : ParamData.GetPropertyBagStruct()->GetPropertyDescs())
	{
		for (int32 Index = 0; Index < ExternalVariables.Num(); Index++)
		{
			FName VariableName = ExternalVariables[Index].Name;
			if (VariableName == FName(FRigVariableIndexCache::MakeFunctionWrapperVariableName(InFunctionName, Desc.Name)))
			{
				ArgumentIndexes.Add(Index);
				break;
			}
		}
	}

	// Search all variables for result, it won't be part of the arg param data
	bool bResultFound = InResultName == NAME_None ? true : false;
	if (!bResultFound)
	{
		for (int32 Index = 0; Index < ExternalVariables.Num(); Index++)
		{
			FName VariableName = ExternalVariables[Index].Name;
			if (VariableName == InResultName)
			{
				ResultIndex = Index;
				bResultFound = true;
				break;
			}
		}
	}

	bool bAllArgsFound = ArgumentIndexes.Num() == ParamData.Num();
	if (bAllArgsFound && bResultFound)
	{
		bInitialized = true;
	}

	// Verify we init first time. We don't want to be spin-attempting to initialize.
	verify(bInitialized);

	return bInitialized;
}

TArrayView<const uint8> FRigVariableIndexCache::GetVMArgumentIndexes() const
{
	return MakeArrayView(ArgumentIndexes);
}

int32 FRigVariableIndexCache::GetVMResultIndex() const
{
	return ResultIndex;
}
