// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SvgDistanceFieldConfiguration.h"

#include "SvgDistanceFieldGenerator.generated.h"

UCLASS()
class SVGDISTANCEFIELD_API USvgDistanceFieldGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

#if WITH_EDITORONLY_DATA
	UFUNCTION(BlueprintCallable, Category=SvgDistanceField)
	static UTexture2D* GenerateTextureFromSvgFile(const FString& SvgFilePath, const FSvgDistanceFieldConfiguration& Configuration);
#endif

};
