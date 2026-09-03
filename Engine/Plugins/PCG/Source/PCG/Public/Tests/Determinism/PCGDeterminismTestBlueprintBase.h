// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PCGDeterminismTestBlueprintBase.generated.h"

#define UE_API PCG_API

struct FDeterminismTestResult;

class UPCGNode;

UCLASS(MinimalAPI, Abstract, BlueprintType, Blueprintable, hidecategories = (Object))
class UPCGDeterminismTestBlueprintBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, CallInEditor, Category = Determinism)
	UE_API void ExecuteTest(const UPCGNode* InPCGNode, UPARAM(ref)FDeterminismTestResult& InOutTestResult);
};

#undef UE_API
