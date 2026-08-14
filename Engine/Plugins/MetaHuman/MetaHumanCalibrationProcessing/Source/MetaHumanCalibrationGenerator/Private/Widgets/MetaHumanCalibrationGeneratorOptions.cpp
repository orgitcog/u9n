// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCalibrationGeneratorOptions.h"

TValueOrError<void, FString> UMetaHumanCalibrationGeneratorOptions::CheckOptionsValidity() const
{
	if (BoardPatternHeight == 0)
	{
		return MakeError(TEXT("Checkerboard pattern height is 0"));
	}

	if (BoardPatternWidth == 0)
	{
		return MakeError(TEXT("Checkerboard pattern width is 0"));
	}

	if (FMath::IsNearlyZero(BoardSquareSize))
	{
		return MakeError(TEXT("Checkerboard square size is 0.0"));
	}

	if (PackagePath.Path.IsEmpty())
	{
		return MakeError(TEXT("Package path is empty"));
	}

	return MakeValue();
}