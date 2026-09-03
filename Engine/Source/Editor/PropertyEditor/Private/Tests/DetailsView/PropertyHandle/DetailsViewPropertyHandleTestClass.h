// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"

#include "DetailsViewPropertyHandleTestClass.generated.h"

#if WITH_EDITORONLY_DATA

UCLASS()
class UDetailsViewPropertyHandleTestValueClass : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class UDetailsViewPropertyHandleTestClass : public UObject
{
	GENERATED_BODY()

public:	
	UPROPERTY(EditAnywhere, Category = "Properties")
	TSoftObjectPtr<UDetailsViewPropertyHandleTestValueClass> TestValueSoftPtr;
	
	UPROPERTY(EditAnywhere, Category = "Properties")
	TArray<TSoftObjectPtr<UDetailsViewPropertyHandleTestValueClass>> TestValueSoftPtrArray;
};

#endif // WITH_EDITORONLY_DATA