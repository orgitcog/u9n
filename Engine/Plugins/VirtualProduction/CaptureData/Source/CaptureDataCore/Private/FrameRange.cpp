// Copyright Epic Games, Inc. All Rights Reserved.

#include "FrameRange.h"

bool FFrameRange::operator==(const FFrameRange& InOther) const
{
	return (Name == InOther.Name && StartFrame == InOther.StartFrame && EndFrame == InOther.EndFrame);
}

bool FFrameRange::ContainsFrame(int32 InFrame, const TArray<FFrameRange>& InFrameRangeArray)
{
	for (int32 Index = 0; Index < InFrameRangeArray.Num(); ++Index)
	{
		const FFrameRange& FrameRange = InFrameRangeArray[Index];

		if ((FrameRange.StartFrame >= 0 || FrameRange.EndFrame >= 0) &&
			(FrameRange.StartFrame < 0 || InFrame >= FrameRange.StartFrame) &&
			(FrameRange.EndFrame < 0 || InFrame <= FrameRange.EndFrame))
		{
			return true;
		}
	}

	return false;
}

TArray<FFrameRange> PackIntoFrameRanges(TArray<FFrameNumber> InFrameNumbers)
{
	TArray<FFrameRange> FrameRanges;

	if (InFrameNumbers.IsEmpty())
	{
		return FrameRanges;
	}

	// Make sure the supplied frame numbers are in the correct order
	InFrameNumbers.Sort();

	TOptional<FFrameNumber> LastFrameNumber;
	TOptional<FFrameNumber> CurrentRangeStart;

	for (const FFrameNumber FrameNumber : InFrameNumbers)
	{
		if (!CurrentRangeStart.IsSet())
		{
			CurrentRangeStart = FrameNumber;
		}

		if (!LastFrameNumber.IsSet())
		{
			LastFrameNumber = FrameNumber;
		}

		if (FrameNumber - *LastFrameNumber > 1)
		{
			FFrameRange NewRange;
			NewRange.StartFrame = CurrentRangeStart->Value;
			NewRange.EndFrame = LastFrameNumber->Value;
			CurrentRangeStart = FrameNumber;

			FrameRanges.Emplace(MoveTemp(NewRange));
		}

		LastFrameNumber = FrameNumber;
	}

	FFrameRange LastRange;
	LastRange.StartFrame = CurrentRangeStart->Value;
	LastRange.EndFrame = LastFrameNumber->Value;
	FrameRanges.Emplace(MoveTemp(LastRange));

	return FrameRanges;
}
