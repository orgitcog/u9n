// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/TVariant.h"
#include "Misc/FrameRate.h"
#include "Misc/Timecode.h"

#include "PlayerTime.h"

namespace ElectraProtronUtils
{

	struct FCodecInfo
	{
		enum class EType
		{
			Video,
			Audio,
			Subtitle,
			Timecode,
			Invalid,
			MAX = Invalid
		};
		struct FVideo
		{
			uint32 Width = 0;
			uint32 Height = 0;
			Electra::FTimeFraction FrameRate;
		};
		struct FAudio
		{
			uint32 NumChannels = 0;
			uint32 ChannelConfiguration = 0;
			uint32 SampleRate = 0;
		};
		struct FSubtitle
		{
		};
		struct FTMCDTimecode
		{
			// See: https://developer.apple.com/documentation/quicktime-file-format/timecode_sample_description/flags
			enum EFlags : uint32
			{
				DropFrame = 0x0001,					// Indicates whether the timecode is drop frame. Set it to 1 if the timecode is drop frame.
				Max24Hour = 0x0002,					// Indicates whether the timecode wraps after 24 hours. Set it to 1 if the timecode wraps.
				AllowNegativeTimes = 0x0004,		// Indicates whether negative time values are allowed. Set it to 1 if the timecode supports negative values.
				Counter = 0x0008					// Indicates whether the time value corresponds to a tape counter value. Set it to 1 if the timecode values are tape counter values.
			};

			uint32 Flags = 0;
			uint32 Timescale = 0;
			uint32 FrameDuration = 0;
			uint32 NumberOfFrames = 0;

			bool IsDropFrame() const
			{ return !!(Flags & EFlags::DropFrame); }

			bool WrapsAfter24Hours() const
			{ return !!(Flags & EFlags::Max24Hour); }

			bool SupportsNegativeTime() const
			{ return !!(Flags & EFlags::AllowNegativeTimes); }

			FFrameRate GetFrameRate() const
			{ return IsDropFrame() ? FFrameRate(Timescale, FrameDuration) : FFrameRate(NumberOfFrames, 1); }

			FTimecode ConvertToTimecode(uint32 InSampleTimecode) const
			{
				// Needs to be an int32 for use with the following methods.
				check(InSampleTimecode <= 0x7fffffffU);
				const FFrameRate FrameRate = GetFrameRate();
				// Convert to time code (apply roll over, etc). via conversion to seconds first.
				return FTimecode(FrameRate.AsSeconds(FFrameTime(FFrameNumber(static_cast<int32>(InSampleTimecode)))), FrameRate, IsDropFrame(), WrapsAfter24Hours());
			}

		};
		EType Type = EType::Invalid;
		FString HumanReadableFormatInfo;
		FString RFC6381;
		uint32 FourCC = 0;
		TVariant<FVideo,FAudio,FSubtitle,FTMCDTimecode> Properties;
		TArray<uint8> DCR;
		TArray<uint8> CSD;
		TMap<uint32, TArray<uint8>> ExtraBoxes;
	};

} // ElectraProtronUtils
