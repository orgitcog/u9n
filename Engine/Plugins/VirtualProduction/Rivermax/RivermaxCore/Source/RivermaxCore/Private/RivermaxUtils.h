// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RivermaxTypes.h"
#include "Misc/Timecode.h"

namespace UE::RivermaxCore
{
	struct FRivermaxOutputOptions;
}

namespace UE::RivermaxCore::Private::Utils
{	
	/** Various constants used for stream initialization */

	static constexpr uint32 FullHDHeight = 1080;
	static constexpr uint32 FullHDWidth = 1920;

	/** SMTPE 2110-10.The Media Clock and RTP Clock rate for streams compliant to this standard shall be 90 kHz. */
	static constexpr double MediaClockSampleRate = 90000.0;
	
	/** Common sleep time used in places where we are waiting for something to complete */
	static constexpr float SleepTimeSeconds = 50 * 1E-6;

	/**
	 * Converts a timestamp in MediaClock period units to a frame number for a given frame rate
	 * 2110-20 streams uses a standard media clock rate of 90kHz
	 */
	uint32 TimestampToFrameNumber(uint32 Timestamp, const FFrameRate& FrameRate);

	/** Returns a mediaclock timestamp, for rtp, based on a clock time */
	uint32 GetTimestampFromTime(uint64 InTimeNanosec, double InMediaClockRate);

	/** Convert PTP time to timecode. */
	FTimecode GetTimecodeFromTime(uint64 InTimeNanosec, double InMediaClockRate, const FFrameRate& FrameRate);
	

}
