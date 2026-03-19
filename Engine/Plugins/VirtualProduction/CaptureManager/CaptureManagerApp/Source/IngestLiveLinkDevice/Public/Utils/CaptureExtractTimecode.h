// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FileMediaSource.h"

#include "MediaSample.h"
#include "MediaPlayer.h"

#include "Templates/ValueOrError.h"

#include "IMediaClock.h"
#include "IMediaClockSink.h"
#include "IMediaModule.h"

#include "CaptureExtractTimecode.generated.h"

namespace UE::CaptureManager
{
struct FTimecodeAndRate
{
	FTimecode Timecode;
	TOptional<FFrameRate> MaybeTimecodeRate;
};

enum ECaptureExtractTimecodeError : int32
{
	InternalError = -1,
	TimecodeNotFound = 1,
	UnableToParseTimecode = 2,
	UnableToParseTimecodeRate = 3,
	TimecodeRateNotFound = 4,
	UnhandledMedia = 5,
	UnableToOpenMedia = 6,
};

using FTimecodeAndRateResult = TValueOrError<FTimecodeAndRate, ECaptureExtractTimecodeError>;
}

#if WITH_EDITOR
UCLASS(BlueprintType, MinimalAPI)
class UDesiredPlayerMediaSource : public UFileMediaSource
{
	GENERATED_BODY()
public:
	//~ IMediaOptions interface
	virtual FName GetDesiredPlayerName() const override
	{
		return TEXT("ElectraPlayer");
	}
};
#endif // WITH_EDITOR

class INGESTLIVELINKDEVICE_API FCaptureExtractVideoTimecode final : public TSharedFromThis<FCaptureExtractVideoTimecode>
{
public:

	FCaptureExtractVideoTimecode(const FString& InFilePath);

	UE::CaptureManager::FTimecodeAndRateResult Extract();

private:

	FString FilePath;

	TValueOrError<FFrameRate, UE::CaptureManager::ECaptureExtractTimecodeError> ParseTimecodeRate(FString TimecodeRateString);

	UE::CaptureManager::FTimecodeAndRateResult ExtractTimecodeUsingElectraPlayer();
	UE::CaptureManager::FTimecodeAndRateResult ExtractTimecodeUsingFFProbe(const FString& InEncoderPath);
	
	const int32 TimeoutPeriod = 3;
};

class INGESTLIVELINKDEVICE_API FCaptureExtractAudioTimecode final : public TSharedFromThis<FCaptureExtractAudioTimecode>
{
public:

	FCaptureExtractAudioTimecode(const FString& InFilePath);
	~FCaptureExtractAudioTimecode() = default;

	UE::CaptureManager::FTimecodeAndRateResult Extract();
	UE::CaptureManager::FTimecodeAndRateResult Extract(FFrameRate InFrameRate);

private:

	FString FilePath;

	const int32 TimeoutPeriod = 3;

	UE::CaptureManager::FTimecodeAndRateResult ExtractTimecodeFromBroadcastWaveFormat(FFrameRate InTimecodeRate);
};
