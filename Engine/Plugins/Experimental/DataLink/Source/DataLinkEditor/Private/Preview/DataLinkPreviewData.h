// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataLinkInstance.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "DataLinkPreviewData.generated.h"

UCLASS()
class UDataLinkPreviewData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Input Data")
	FDataLinkInstance DataLinkInstance;

	UPROPERTY(VisibleAnywhere, Category="Output Data", meta=(StructTypeConst))
	FInstancedStruct OutputData;
};
