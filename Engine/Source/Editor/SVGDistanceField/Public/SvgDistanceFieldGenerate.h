// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DistanceFieldImage.h"
#include "SvgDistanceFieldConfiguration.h"

bool SVGDISTANCEFIELD_API SvgDistanceFieldGenerate(TArrayView64<const char> InSvgData, const FSvgDistanceFieldConfiguration& InConfiguration, FDistanceFieldImage& OutDistanceFieldImage);
