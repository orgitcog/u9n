// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MetaHumanMeshData.generated.h"


USTRUCT()
struct METAHUMANCORETECH_API FMetaHumanMeshData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<float> FaceMeshVertData;

	UPROPERTY()
	TArray<float> TeethMeshVertData;

	UPROPERTY()
	TArray<float> LeftEyeMeshVertData;

	UPROPERTY()
	TArray<float> RightEyeMeshVertData;
};
