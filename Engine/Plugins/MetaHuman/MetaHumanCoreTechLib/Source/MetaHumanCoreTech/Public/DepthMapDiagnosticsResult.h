// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DepthMapDiagnosticsResult.generated.h"

USTRUCT()
struct METAHUMANCORETECH_API FDepthMapDiagnosticsResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Diagnostics")
	int32 NumFacePixels = 0;

	UPROPERTY(VisibleAnywhere, Category = "Diagnostics")
	int32 NumFaceValidDepthMapPixels = 0;

	UPROPERTY(VisibleAnywhere, Category = "Diagnostics")
	float FaceWidthInPixels = 0.0f; 

};
