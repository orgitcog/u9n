// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CameraCalibration.h"

class DATAINGESTCORE_API FUnrealCalibrationParser
{
public:
	using FParseResult = TValueOrError<TArray<FCameraCalibration>, FText>;

	/** Parse input file into unreal calibration format. */
	static FParseResult Parse(const FString& InFile);
};