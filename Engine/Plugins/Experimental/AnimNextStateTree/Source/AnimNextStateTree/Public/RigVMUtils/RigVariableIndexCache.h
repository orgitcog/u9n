// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextStateTreeTypes.h"

class UAnimNextDataInterface;
struct FRigVMMemoryStorageStruct;

/** 
 * Helper struct to store parent RigVM variable indicies,.
 * 
 * This is needed as the internal variables we read / write will be offset somewhere after the public vars in the data interface.
 */
struct FRigVariableIndexCache
{

public:

	// @TODO: Relocate, must be in sync with UncookedOnlyUtils.h
	/** Make a variable name that we use as a wrapper for a function param or return */
	static FString MakeFunctionWrapperVariableName(FName InFunctionName, FName InVariableName);

	// @TODO: Relocate, must be in sync with UncookedOnlyUtils.h
	/** Make an event name that we use as a wrapper to call RigVM functions */
	static FString MakeFunctionWrapperEventName(FName InFunctionName);

public:

	/** True if VM variables are indexed */
	bool IsIndexCacheInitialized() const;

	/** True if successfully populated index cache from Data Interface, or already populated*/
	bool TryPopulateIndexCache(const FRigVMMemoryStorageStruct& ParamData, const UAnimNextDataInterface* InDataInterface, const FName InFunctionName, const FName InResultName = NAME_None);

	/** Gets Argument Indexes, you are responsible for checking if initialized first */
	TArrayView<const uint8> GetVMArgumentIndexes() const;

	/** Gets Result Index, you are responsible for checking if initialized first */
	int32 GetVMResultIndex() const;

protected:

	TArray<uint8, TInlineAllocator<8>> ArgumentIndexes = {};
	int32 ResultIndex = INDEX_NONE;
	bool bInitialized = false;
};