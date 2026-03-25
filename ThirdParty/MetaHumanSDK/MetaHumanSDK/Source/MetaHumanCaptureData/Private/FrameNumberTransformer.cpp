// Copyright Epic Games, Inc. All Rights Reserved.

#include "FrameNumberTransformer.h"

namespace UE::MetaHuman
{

FFrameNumberTransformer::FFrameNumberTransformer() :
	SourceFrameRate(FFrameRate(1, 1)),
	TargetFrameRate(FFrameRate(1, 1))
{
}

FFrameNumberTransformer::FFrameNumberTransformer(const int32 InFrameNumberOffset) :
	SourceFrameRate(FFrameRate(1, 1)),
	TargetFrameRate(FFrameRate(1, 1)),
	FrameNumberOffset(InFrameNumberOffset)
{
}

FFrameNumberTransformer::FFrameNumberTransformer(FFrameRate InSourceFrameRate, FFrameRate InTargetFrameRate) :
	SourceFrameRate(MoveTemp(InSourceFrameRate)),
	TargetFrameRate(MoveTemp(InTargetFrameRate))
{
	const double Ratio = FMath::Abs(SourceFrameRate.AsDecimal() / TargetFrameRate.AsDecimal());

	if (!FMath::IsNearlyZero(Ratio))
	{
		SkipFactor = Ratio;
		DuplicationFactor = 1.0 / Ratio;
	}
}

FFrameNumberTransformer::FFrameNumberTransformer(FFrameRate InSourceFrameRate, FFrameRate InTargetFrameRate, const int32 InFrameNumberOffset) :
	SourceFrameRate(MoveTemp(InSourceFrameRate)),
	TargetFrameRate(MoveTemp(InTargetFrameRate)),
	FrameNumberOffset(InFrameNumberOffset)
{
	const double Ratio = FMath::Abs(SourceFrameRate.AsDecimal() / TargetFrameRate.AsDecimal());

	if (!FMath::IsNearlyZero(Ratio))
	{
		SkipFactor = Ratio;
		DuplicationFactor = 1.0 / Ratio;
	}
}

int32 FFrameNumberTransformer::Transform(const int32 InFrameNumber) const
{
	double NewFrameNumber = FrameNumberOffset + InFrameNumber;

	if (DuplicationFactor > 1)
	{
		NewFrameNumber /= DuplicationFactor;
	}
	else if (SkipFactor > 1)
	{
		NewFrameNumber *= SkipFactor;
	}

	return FMath::TruncToInt32(NewFrameNumber);
}

}
