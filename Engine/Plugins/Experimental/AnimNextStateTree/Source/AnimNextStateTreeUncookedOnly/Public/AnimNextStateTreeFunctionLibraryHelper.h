// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextStateTreeFunctionLibraryHelper.generated.h"

struct FRigVMGraphFunctionHeader;

/**
 * Helper class for using RigVM functions in ST
 * 
 * Currently used for GetOptions funciton population
 */
UCLASS()
class ANIMNEXTSTATETREEUNCOOKEDONLY_API UAnimNextStateTreeFunctionLibraryHelper : public UObject
{
	GENERATED_BODY()

public:

	// Get list of function names in module
	UFUNCTION()
	static const TArray<FName> GetExposedAnimNextFunctionNames();
};