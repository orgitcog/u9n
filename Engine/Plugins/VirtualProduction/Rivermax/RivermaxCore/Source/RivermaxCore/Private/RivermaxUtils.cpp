// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxUtils.h"

#include "HAL/IConsoleManager.h"
#include "RivermaxLog.h"
#include "RivermaxTypes.h"


namespace UE::RivermaxCore::Private::Utils
{
	uint32 TimestampToFrameNumber(uint32 Timestamp, const FFrameRate& FrameRate)
	{
		using namespace UE::RivermaxCore::Private::Utils;
		const double MediaFrameTime = Timestamp / MediaClockSampleRate;
		const uint32 FrameNumber = FMath::RoundToInt(MediaFrameTime * FrameRate.AsDecimal());
		return FrameNumber;
	}

	/** Returns a mediaclock timestamp, for rtp, based on a clock time */
	uint32 GetTimestampFromTime(uint64 InTimeNanosec, double InMediaClockRate)
	{
		// RTP timestamp is 32 bits and based on media clock (usually 90kHz).
		// Conversion based on rivermax samples

		const uint64 Nanoscale = 1E9;
		const uint64 Seconds = InTimeNanosec / Nanoscale;
		const uint64 Nanoseconds = InTimeNanosec % Nanoscale;
		const uint64 MediaFrameNumber = Seconds * InMediaClockRate;
		const uint64 MediaSubFrameNumber = Nanoseconds * InMediaClockRate / Nanoscale;
		const double Mask = 0x100000000;
		const double MediaTime = FMath::Fmod(MediaFrameNumber, Mask);
		const double MediaSubTime = FMath::Fmod(MediaSubFrameNumber, Mask);
		return MediaTime + MediaSubTime;
	}

	FTimecode GetTimecodeFromTime(uint64 InTimeNanosec, double InMediaClockRate, const FFrameRate& FrameRate)
	{
		FTimespan Timespan = FTimespan(InTimeNanosec / ETimespan::NanosecondsPerTick);

		// This should come from timecode provider or media profile.
		constexpr int32 DaylightSavingTimeHourOffset = 0; 
		constexpr int32 UTCSecondsOffset = 37;

		// Adjust for daylight saving that might be required. 
		Timespan -= FTimespan(DaylightSavingTimeHourOffset, 0, 0);

		// Convert from TAI PTP Time to UTC
		Timespan -= FTimespan(0, 0, UTCSecondsOffset);

		constexpr bool bRollOver = true;

		return FTimecode::FromTimespan(Timespan, FrameRate, bRollOver);
	}
}