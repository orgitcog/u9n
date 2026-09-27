// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace epic
{
namespace core
{

void METAHUMANCORETECH_API BurnPointsIntoImage(const TArray<FVector2D>& InPoints, int32 InWidth, int32 InHeight, TArray<uint8>& InImgData, uint8 InRed, uint8 InGreen, uint8 InBlue, int32 InSize, bool bInUseAntiAliasing = true);

void METAHUMANCORETECH_API BurnPointsIntoImage(const TArray<FVector2D>& InPoints, int32 InWidth, int32 InHeight, FColor* OutImgData, uint8 InRed, uint8 InGreen, uint8 InBlue, int32 InSize, bool bInUseAntiAliasing = true);

void METAHUMANCORETECH_API BurnLineIntoImage(const FVector2D& InStartPoint, const FVector2D& InEndPoint, int32 InWidth, int32 InHeight, TArray<uint8>& OutImgData, uint8 InRed, uint8 InGreen, uint8 InBlue, int32 InLineWidth, bool bInUseAntiAliasing = true);

void METAHUMANCORETECH_API BurnLineIntoImage(const FVector2D& InStartPoint, const FVector2D& InEndPoint, int32 InWidth, int32 InHeight, FColor* OutImgData, uint8 InRed, uint8 InGreen, uint8 InBlue, int32 InLineWidth, bool bInUseAntiAliasing = true);


}
}
