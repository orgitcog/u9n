// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencedImageTrackInfo.h"

namespace UE::MetaHuman
{

FSequencedImageTrackInfo::FSequencedImageTrackInfo(FFrameRate InSourceFrameRate, TRange<FFrameNumber> InSequenceFrameRange) :
	SourceFrameRate(MoveTemp(InSourceFrameRate)),
	SequenceFrameRange(MoveTemp(InSequenceFrameRange))
{
}

FSequencedImageTrackInfo::~FSequencedImageTrackInfo() = default;

FFrameRate FSequencedImageTrackInfo::GetSourceFrameRate() const
{
	return SourceFrameRate;
}

TRange<FFrameNumber> FSequencedImageTrackInfo::GetSequenceFrameRange() const
{
	return SequenceFrameRange;
}

bool FrameRatesAreCompatible(const FFrameRate InFirstFrameRate, const FFrameRate InSecondFrameRate)
{
	if (InFirstFrameRate == InSecondFrameRate)
	{
		return true;
	}

	const double FirstFrameRate = InFirstFrameRate.AsDecimal();
	const double SecondFrameRate = InSecondFrameRate.AsDecimal();
	const double MaxFrameRate = FMath::Max(FirstFrameRate, SecondFrameRate);
	const double MinFrameRate = FMath::Min(FirstFrameRate, SecondFrameRate);

	if (MinFrameRate <= UE_SMALL_NUMBER)
	{
		// One of the frame rates is zero, one is not - incompatible
		return false;
	}

	const double Remainder = FMath::Fmod(MaxFrameRate, MinFrameRate);
	const bool bRemainderIsNearlyZero = FMath::IsNearlyZero(Remainder);

	return bRemainderIsNearlyZero;
}

bool TracksHaveCompatibleFrameRates(const TArray<FSequencedImageTrackInfo>& InSequencedImageTrackInfos)
{
	if (InSequencedImageTrackInfos.Num() < 2)
	{
		return true;
	}

	for (int32 FirstIndex = 0; FirstIndex < InSequencedImageTrackInfos.Num(); ++FirstIndex)
	{
		const FFrameRate FirstFrameRate = InSequencedImageTrackInfos[FirstIndex].GetSourceFrameRate();

		for (int32 SecondIndex = FirstIndex + 1; SecondIndex < InSequencedImageTrackInfos.Num(); ++SecondIndex)
		{
			const FFrameRate SecondFrameRate = InSequencedImageTrackInfos[SecondIndex].GetSourceFrameRate();

			if (!FrameRatesAreCompatible(FirstFrameRate, SecondFrameRate))
			{
				return false;
			}
		}
	}

	return true;
}

bool TracksHaveDifferentFrameRates(const TArray<UE::MetaHuman::FSequencedImageTrackInfo>& InSequencedImageTrackInfos)
{
	if (InSequencedImageTrackInfos.Num() < 2)
	{
		return false;
	}

	for (int32 FirstIndex = 0; FirstIndex < InSequencedImageTrackInfos.Num(); ++FirstIndex)
	{
		const FFrameRate FirstFrameRate = InSequencedImageTrackInfos[FirstIndex].GetSourceFrameRate();

		for (int32 SecondIndex = FirstIndex + 1; SecondIndex < InSequencedImageTrackInfos.Num(); ++SecondIndex)
		{
			const FFrameRate SecondFrameRate = InSequencedImageTrackInfos[SecondIndex].GetSourceFrameRate();

			if (SecondFrameRate != FirstFrameRate)
			{
				return true;
			}
		}
	}

	return false;
}

int32 FindFirstCommonFrameNumber(TArray<UE::MetaHuman::FSequencedImageTrackInfo> InSequencedImageTrackInfos)
{
	int32 FirstFrameInAllTracks = -1;

	if (InSequencedImageTrackInfos.IsEmpty())
	{
		return FirstFrameInAllTracks;
	}

	int32 HighestLowerBound = -1;
	int32 HighestUpperBound = -1;

	double HighestFrameRate = -1;
	double LowestFrameRate = std::numeric_limits<int32>::max();

	TPair<int32, double> LowerBoundAtLowestFrameRate = { std::numeric_limits<int32>::max(), std::numeric_limits<int32>::max() };

	for (const FSequencedImageTrackInfo& SequencedImageTrackInfo : InSequencedImageTrackInfos)
	{
		double FrameRate = SequencedImageTrackInfo.GetSourceFrameRate().AsDecimal();
		int32 LowerBound = SequencedImageTrackInfo.GetSequenceFrameRange().GetLowerBoundValue().Value;
		
		if (FrameRate < LowerBoundAtLowestFrameRate.Value)
		{
			LowerBoundAtLowestFrameRate.Value = FrameRate;
			LowerBoundAtLowestFrameRate.Key = LowerBound;
		}
		else if (FrameRate == LowerBoundAtLowestFrameRate.Value)
		{
			if (LowerBound < LowerBoundAtLowestFrameRate.Key)
			{
				LowerBoundAtLowestFrameRate.Key = LowerBound;
			}
		}

		if (FrameRate < LowestFrameRate)
		{
			// Keep track of lowest frame rate
			LowestFrameRate = FrameRate;
		}

		if (FrameRate > HighestFrameRate)
		{
			// Keep track of highest frame rate
			HighestFrameRate = FrameRate;
		}
	}

	for (const FSequencedImageTrackInfo& SequencedImageTrackInfo : InSequencedImageTrackInfos)
	{
		HighestLowerBound = FMath::Max(HighestLowerBound, SequencedImageTrackInfo.GetSequenceFrameRange().GetLowerBoundValue().Value);
		HighestUpperBound = FMath::Max(HighestUpperBound, SequencedImageTrackInfo.GetSequenceFrameRange().GetUpperBoundValue().Value);
	}

	// Calculate the highest frame rate ratio
	int32 MaxFrameRateRatio = -1;
	MaxFrameRateRatio = FMath::Max(MaxFrameRateRatio, FMath::Abs(HighestFrameRate / LowestFrameRate));



	// Anything below the highest lower bound frame number will not exist in all tracks (by definition) so we can start our search here
	int32 FrameNumber = HighestLowerBound;

	if (FrameNumber > LowerBoundAtLowestFrameRate.Key)
	{
		// We have dropped frames in the lower frame rate track
		// So need to check if we need to skip frames at the start
		
		// How many frames will we drop from the start
		int32 DeltaFromLowerBoundToHigestLowerBound = HighestLowerBound - LowerBoundAtLowestFrameRate.Key;

		// Check whether this number aligns with the frame rate ratio or whether we need to skip additional frames
		int32 Offset = DeltaFromLowerBoundToHigestLowerBound % MaxFrameRateRatio;

		// Calculate skip factor
		const int32 NumFramesToSkip = MaxFrameRateRatio - Offset;

		// Offset the first frame by the skip factor if required
		//int32 Add = 
		FrameNumber += Offset == 0 ? 0 : NumFramesToSkip;
	}

	while (FrameNumber <= HighestUpperBound)
	{
		int32 NumTracksWithThisFrameNumber = 0;

		for (const FSequencedImageTrackInfo& SequencedImageTrackInfo : InSequencedImageTrackInfos)
		{
			const int32 FrameNumberLow = SequencedImageTrackInfo.GetSequenceFrameRange().GetLowerBoundValue().Value;
			const int32 FrameNumberHigh = SequencedImageTrackInfo.GetSequenceFrameRange().GetUpperBoundValue().Value;

			if (FrameNumber >= FrameNumberLow && FrameNumber <= FrameNumberHigh)
			{
				++NumTracksWithThisFrameNumber;
			}
		}

		if (NumTracksWithThisFrameNumber == InSequencedImageTrackInfos.Num())
		{
			FirstFrameInAllTracks = FrameNumber;
			break;
		}

		++FrameNumber;
	}

	return FirstFrameInAllTracks;
}

TArray<FFrameNumber> CalculateRateMatchingDropFrames(
	FFrameRate InTargetFrameRate,
	TArray<UE::MetaHuman::FSequencedImageTrackInfo> InSequencedImageTrackInfos
)
{
	TArray<FFrameNumber> DropFrames;

	if (InSequencedImageTrackInfos.IsEmpty())
	{
		return DropFrames;
	}

	int32 MinFrameNumber = std::numeric_limits<int32>::max();
	int32 MaxFrameNumber = -1;
	int32 MaxFrameRateRatio = -1;

	for (const FSequencedImageTrackInfo& SequencedImageTrackInfo : InSequencedImageTrackInfos)
	{
		MinFrameNumber = FMath::Min(MinFrameNumber, SequencedImageTrackInfo.GetSequenceFrameRange().GetLowerBoundValue().Value);
		MaxFrameNumber = FMath::Max(MaxFrameNumber, SequencedImageTrackInfo.GetSequenceFrameRange().GetUpperBoundValue().Value);
		MaxFrameRateRatio = FMath::Max(MaxFrameRateRatio, FMath::Abs(InTargetFrameRate.AsDecimal() / SequencedImageTrackInfo.GetSourceFrameRate().AsDecimal()));
	}

	const int32 FirstCommonFrameNumber = FindFirstCommonFrameNumber(InSequencedImageTrackInfos);

	if (FirstCommonFrameNumber > MinFrameNumber)
	{
		// The tracks are not already aligned, so we mark all frames leading up to the first common frame as dropped
		for (int32 FrameNumber = MinFrameNumber; FrameNumber < FirstCommonFrameNumber; ++FrameNumber)
		{
			DropFrames.Emplace(FrameNumber);
		}
	}

	if (MaxFrameRateRatio > 1)
	{
		int32 FrameNumber = FirstCommonFrameNumber + 1;
		const int32 NumFramesToSkip = MaxFrameRateRatio - 1;
		int32 NumSkipsRemaining = NumFramesToSkip;

		while (FrameNumber <= MaxFrameNumber)
		{
			if (NumSkipsRemaining > 0)
			{
				DropFrames.Emplace(FrameNumber);
				--NumSkipsRemaining;
			}
			else
			{
				NumSkipsRemaining = NumFramesToSkip;
			}

			++FrameNumber;
		}
	}

	return DropFrames;
}

TArray<FFrameNumber> CalculateRateMatchingDropFrames(
	FFrameRate InTargetFrameRate,
	TArray<UE::MetaHuman::FSequencedImageTrackInfo> InSequencedImageTrackInfos,
	const TRange<FFrameNumber>& InRangeLimit
)
{
	TArray<FFrameNumber> DropFrames = CalculateRateMatchingDropFrames(InTargetFrameRate, MoveTemp(InSequencedImageTrackInfos));
	TArray<FFrameNumber> DropFramesInRange;

	for (const FFrameNumber FrameNumber : DropFrames)
	{
		if (FrameNumber >= InRangeLimit.GetLowerBoundValue() && FrameNumber <= InRangeLimit.GetUpperBoundValue())
		{
			DropFramesInRange.Emplace(FrameNumber);
		}
	}

	return DropFramesInRange;
}



}
