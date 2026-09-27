// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "SerializableEvaluationProgram.generated.h"

namespace UE::AnimNext
{
	struct FEvaluationProgram;
}

USTRUCT()
struct FSerializableEvaluationProgram
{
	GENERATED_BODY()
	
	FSerializableEvaluationProgram()
	{
	}
	
	FSerializableEvaluationProgram(const UE::AnimNext::FEvaluationProgram& Other);
	
	UPROPERTY(EditAnywhere, Category = "Tasks", meta = (ExpandByDefault))
	TArray<FInstancedStruct> Tasks;	
};
