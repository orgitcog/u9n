// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "OptimusFunctionNodeGraphHeaderWithGuid.generated.h"

class UOptimusNode_FunctionReference;

USTRUCT()
struct FOptimusFunctionNodeGraphHeaderWithGuid
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid FunctionGraphGuid;

	UPROPERTY()
	FName FunctionName;

	UPROPERTY()
	FName Category;
};

USTRUCT()
struct FOptimusFunctionNodeGraphHeaderWithGuidArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FOptimusFunctionNodeGraphHeaderWithGuid> Headers;
};

