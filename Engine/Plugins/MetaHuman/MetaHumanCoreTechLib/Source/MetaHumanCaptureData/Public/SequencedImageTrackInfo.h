// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"

class UImgMediaSource;

namespace UE::MetaHuman
{

/** Brings together some sequencer and image sequence information for a specific track (A Sequenced "Image Track") */
class METAHUMANCAPTUREDATA_API FSequencedImageTrackInfo
{
public:
	FSequencedImageTrackInfo(FFrameRate InSourceFrameRate, TRange<FFrameNumber> InSequenceFrameRange);
	~FSequencedImageTrackInfo();

	FFrameRate GetSourceFrameRate() const;
	TRange<FFrameNumber> GetSequenceFrameRange() const;

private:
	FFrameRate SourceFrameRate;
	TRange<FFrameNumber> SequenceFrameRange;
};

METAHUMANCAPTUREDATA_API bool FrameRatesAreCompatible(FFrameRate InFirstFrameRate, FFrameRate InSecondFrameRate);
METAHUMANCAPTUREDATA_API bool TracksHaveCompatibleFrameRates(const TArray<FSequencedImageTrackInfo>& InSequencedImageTrackInfos);
METAHUMANCAPTUREDATA_API bool TracksHaveDifferentFrameRates(const TArray<FSequencedImageTrackInfo>& InSequencedImageTrackInfos);
METAHUMANCAPTUREDATA_API int32 FindFirstCommonFrameNumber(TArray<UE::MetaHuman::FSequencedImageTrackInfo> InSequencedImageTrackInfos);

METAHUMANCAPTUREDATA_API TArray<FFrameNumber> CalculateRateMatchingDropFrames(
	FFrameRate InTargetFrameRate,
	TArray<UE::MetaHuman::FSequencedImageTrackInfo> InSequencedImageTrackInfos
);

METAHUMANCAPTUREDATA_API TArray<FFrameNumber> CalculateRateMatchingDropFrames(
	FFrameRate InTargetFrameRate,
	TArray<UE::MetaHuman::FSequencedImageTrackInfo> InSequencedImageTrackInfos,
	const TRange<FFrameNumber>& InRangeLimit
);

}
