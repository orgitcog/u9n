// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OptimusShaderText.generated.h"

USTRUCT()
struct FOptimusShaderText
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category=ShaderText)
	FString Declarations;
	
	UPROPERTY(EditAnywhere, Category=ShaderText)
	FString ShaderText;
};
