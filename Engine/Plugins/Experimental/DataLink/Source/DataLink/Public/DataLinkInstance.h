// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "StructUtils/InstancedStruct.h"
#include "DataLinkInstance.generated.h"

class UDataLinkGraph;

/** Instance of a data link to be executed */
USTRUCT(BlueprintType)
struct FDataLinkInstance
{
	GENERATED_BODY()

	/** The data link graph to execute */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Link")
	TObjectPtr<UDataLinkGraph> DataLinkGraph;

	/** The initial input data to feed into the data link graph */
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category="Data Link")
	TArray<FInstancedStruct> InputData;
};
