// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "ContourDataComparisonHelper.generated.h"

UCLASS(transient)
class UContourDataComparisonHelper : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Testing Helper Tools")
	static bool ComparePerformanceContourData(const UMetaHumanPerformance* InOriginal, const UMetaHumanPerformance* InNew);

private:

	static float ContourComparisonTolerance;
};