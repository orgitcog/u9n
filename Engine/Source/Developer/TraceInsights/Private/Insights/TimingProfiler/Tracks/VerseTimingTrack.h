// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

// TraceInsights
#include "Insights/TimingProfiler/Tracks/ThreadTimingTrackPrivate.h"

namespace UE::Insights::TimingProfiler
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FVerseTimingTrack : public FThreadTimingTrackImpl
{
	INSIGHTS_DECLARE_RTTI(FVerseTimingTrack, FThreadTimingTrackImpl)

public:
	static constexpr uint32 VerseThreadId = uint32('VRSE');

	explicit FVerseTimingTrack(FThreadTimingSharedState& InSharedState, const FString& InName, uint32 InTimelineIndex)
		: FThreadTimingTrackImpl(InSharedState, InName, nullptr, InTimelineIndex, VerseThreadId)
	{
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace UE::Insights::TimingProfiler
