// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/CaptureExtractTimecode.h"

#include "Audio.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "MediaPlaylist.h"
#include "Sound/SoundWaveTimecodeInfo.h"
#include "UObject/Package.h"
#include "Settings/CaptureManagerSettings.h"
#include "Logging/LogMacros.h"
#include "IElectraPlayerPluginModule.h"
#include "IMediaPlayer.h"
#include "IMediaOptions.h"
#include "IMediaEventSink.h"

#define LOCTEXT_NAMESPACE "CaptureExtractTimecode"

DEFINE_LOG_CATEGORY_STATIC(LogCaptureExtractTimecode, Log, All);

FCaptureExtractVideoTimecode::FCaptureExtractVideoTimecode(const FString& InFilePath)
	: FilePath(InFilePath)
{}

UE::CaptureManager::FTimecodeAndRateResult FCaptureExtractVideoTimecode::Extract()
{
	using namespace UE::CaptureManager;

	check(!FilePath.IsEmpty())

	FTimecodeAndRateResult Result = MakeError(ECaptureExtractTimecodeError::UnhandledMedia);

	Result = ExtractTimecodeUsingElectraPlayer();

	if (!Result.IsValid())
	{
		const UCaptureManagerSettings* Settings = GetDefault<UCaptureManagerSettings>();

		if (Settings->bEnableThirdPartyEncoder)
		{
			FString EncoderPath = Settings->ThirdPartyEncoder.FilePath;
			if (EncoderPath.EndsWith(TEXT("ffmpeg.exe")))
			{
				EncoderPath.ReplaceInline(TEXT("ffmpeg.exe"), TEXT("ffprobe.exe"));
				Result = ExtractTimecodeUsingFFProbe(EncoderPath);
			}
		}
	}

	if (Result.HasValue())
	{
		UE_LOG(LogCaptureExtractTimecode, Display, TEXT("Extracted timecode '%s' from video file: %s"), *Result.GetValue().Timecode.ToString(), *FilePath);
	}
	else
	{
		UE_LOG(LogCaptureExtractTimecode, Display, TEXT("Timecode not found for video file: %s"), *FilePath);
	}

	return Result;
}

UE::CaptureManager::FTimecodeAndRateResult FCaptureExtractVideoTimecode::ExtractTimecodeUsingElectraPlayer()
{
	using namespace UE::CaptureManager;

	using FEventSinkResult = TValueOrError<void, ECaptureExtractTimecodeError>;
	TPromise<FEventSinkResult> EventSinkPromise;

	class FMediaEventSink : public IMediaEventSink
	{
	public:

		FMediaEventSink(TPromise<FEventSinkResult>& InPromise)
			: Promise(InPromise)
			, bReleased(false)
		{
		}

		virtual void ReceiveMediaEvent(EMediaEvent Event) override
		{
			if (bReleased)
			{
				return;
			}

			if (Event == EMediaEvent::MediaOpened)
			{
				Promise.SetValue(MakeValue());
			}
			else if (Event == EMediaEvent::MediaOpenFailed)
			{
				Promise.SetValue(MakeError(ECaptureExtractTimecodeError::UnableToOpenMedia));
			}
		}

		void Release()
		{
			bReleased = true;
		}

	private:

		TPromise<FEventSinkResult>& Promise;
		std::atomic_bool bReleased;

	} MediaEventSink(EventSinkPromise);

	IElectraPlayerPluginModule& ElectraModule = FModuleManager::LoadModuleChecked<IElectraPlayerPluginModule>("ElectraPlayerPlugin");

	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> MediaPlayer = ElectraModule.CreatePlayer(MediaEventSink);
	if (!MediaPlayer)
	{
		// Broken promises are considered errors
		UE::CaptureManager::FTimecodeAndRateResult Result = MakeError(ECaptureExtractTimecodeError::InternalError);
		EventSinkPromise.SetValue(MakeError(Result.GetError()));
		return Result;
	}

	UDesiredPlayerMediaSource* MediaSource = NewObject<UDesiredPlayerMediaSource>();
	MediaSource->SetFilePath(FilePath);

	FString FileUrl = TEXT("file://") + FilePath;

	FMediaPlayerOptions PlayerOptions;
	PlayerOptions.SetAllAsOptional();
	PlayerOptions.InternalCustomOptions.Emplace(MediaPlayerOptionValues::ParseTimecodeInfo(), FVariant());

	MediaPlayer->Open(FileUrl, MediaSource, &PlayerOptions);

	FString TimecodeString;
	FString TimecodeRateString;

	FDateTime EventWaitStart = FDateTime::Now();

	TFuture<FEventSinkResult> EventSinkFuture = EventSinkPromise.GetFuture();
	
	while ((FDateTime::Now() - EventWaitStart).GetSeconds() < TimeoutPeriod)
	{
		MediaPlayer->TickInput(0, 0);

		if (EventSinkFuture.WaitFor(FTimespan::FromMilliseconds(100.0f))) // Wait 100 milliseconds before invoking TickInput again
		{
			FEventSinkResult Result = EventSinkFuture.Get();
			if (Result.HasError())
			{
				return MakeError(Result.StealError());
			}

			const FVariant Timecode = MediaPlayer->GetMediaInfo(UMediaPlayer::MediaInfoNameStartTimecodeValue.Resolve());
			if (!Timecode.IsEmpty())
			{
				TimecodeString = Timecode.GetValue<FString>();

				const FVariant TimecodeRate = MediaPlayer->GetMediaInfo(UMediaPlayer::MediaInfoNameStartTimecodeFrameRate.Resolve());
				if (!TimecodeRate.IsEmpty())
				{
					TimecodeRateString = TimecodeRate.GetValue<FString>();
				}
			}

			break;
		}
	}

	// Making sure that no additional events will arrive
	MediaEventSink.Release();
	MediaPlayer->Close();

	if (TimecodeString.IsEmpty())
	{
		UE::CaptureManager::FTimecodeAndRateResult Result = MakeError(ECaptureExtractTimecodeError::TimecodeNotFound);

		if (!EventSinkFuture.IsReady())
		{
			// Broken promises are considered errors
			EventSinkPromise.SetValue(MakeError(Result.GetError()));
		}

		return Result;
	}
	
	const TOptional<FTimecode> Timecode = FTimecode::ParseTimecode(*TimecodeString);
	if (!Timecode.IsSet())
	{
		return MakeError(ECaptureExtractTimecodeError::UnableToParseTimecode);
	}

	TOptional<FFrameRate> MaybeTimecodeRate;

	if (!TimecodeRateString.IsEmpty())
	{
		TValueOrError<FFrameRate, UE::CaptureManager::ECaptureExtractTimecodeError> Result = ParseTimecodeRate(TimecodeRateString);
		if (Result.IsValid())
		{
			MaybeTimecodeRate = Result.GetValue();
		}
	}

	return MakeValue(Timecode.GetValue(), MoveTemp(MaybeTimecodeRate));
}

TValueOrError<FFrameRate, UE::CaptureManager::ECaptureExtractTimecodeError> FCaptureExtractVideoTimecode::ParseTimecodeRate(FString TimecodeRateString)
{
	using namespace UE::CaptureManager;

	if (TimecodeRateString.IsEmpty())
	{
		return MakeError(ECaptureExtractTimecodeError::TimecodeRateNotFound);
	}
	else
	{
		// TimecodeRateString is made out of FFrameRate::ToPrettyText()
		// It could be "{0} fps" or "{0} s"

		FFrameRate TimecodeRate;

		FString Left;
		FString Right;
		TimecodeRateString.Split(" ", &Left, &Right);

		double Number = FCString::Atoi(*Left);
		uint32 IntNumber;
		uint32 Multiplier = 1;


		for (; Multiplier <= 10000; Multiplier *= 10)
		{
			double TmpNumber = Number * Multiplier;
			IntNumber = FMath::RoundToInt32(TmpNumber);
			if (FMath::Abs(TmpNumber - IntNumber) < 0.01)
			{
				break;
			}
		}

		uint32 Nominator = 0;
		uint32 Denominator = 0;

		if (Right == "fps")
		{
			Nominator = IntNumber;
			Denominator = Multiplier;
		}
		else if (Right == "s")
		{
			Nominator = Multiplier;
			Denominator = IntNumber;
		}

		TimecodeRate = FFrameRate(Nominator, Denominator);

		return MakeValue(TimecodeRate);
	}
}

UE::CaptureManager::FTimecodeAndRateResult FCaptureExtractVideoTimecode::ExtractTimecodeUsingFFProbe(const FString& InFFProbePath)
{
	using namespace UE::CaptureManager;

	FTimecodeAndRateResult Result = MakeError(ECaptureExtractTimecodeError::UnhandledMedia);

	FString CommandArgs = TEXT("-v error -select_streams v:0 -show_entries stream_tags=timecode:stream=r_frame_rate -of default=noprint_wrappers=1:nokey=1 ") + FilePath;

	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	verify(FPlatformProcess::CreatePipe(ReadPipe, WritePipe, false));

	constexpr bool bLaunchDetached = false;
	constexpr bool bLaunchHidden = true;
	constexpr bool bLaunchReallyHidden = true;
	FProcHandle ProcHandle =
		FPlatformProcess::CreateProc(*InFFProbePath, *CommandArgs, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, nullptr, 0, nullptr, WritePipe, nullptr);

	ON_SCOPE_EXIT
	{
		FPlatformProcess::TerminateProc(ProcHandle);
		FPlatformProcess::CloseProc(ProcHandle);
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
	};

	if (!ProcHandle.IsValid())
	{
		UE_LOG(LogCaptureExtractTimecode, Display, TEXT("Extract timecode: Failed to start the process %s %s"), *InFFProbePath, *CommandArgs);
		return MakeError(ECaptureExtractTimecodeError::InternalError);
	}

	const FDateTime WaitStart = FDateTime::Now();

	TArray<uint8> FullCommandOutput;
	while (FPlatformProcess::IsProcRunning(ProcHandle))
	{
		TArray<uint8> CommandOutput;
		{
			bool Read = FPlatformProcess::ReadPipeToArray(ReadPipe, CommandOutput);
			if (!Read)
			{
				CommandOutput.Empty();
			}
		}

		if ((FDateTime::Now() - WaitStart).GetSeconds() > TimeoutPeriod)
		{
			// Timed out
			break;
		}

		if (CommandOutput.IsEmpty())
		{
			FPlatformProcess::Sleep(0.1f);
			continue;
		}

		FullCommandOutput.Append(MoveTemp(CommandOutput));

	}

	int32 ReturnCode = 0;
	FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);


	TArray<uint8> CommandOutput;
	{
		bool Read = FPlatformProcess::ReadPipeToArray(ReadPipe, CommandOutput);
		if (!Read)
		{
			CommandOutput.Empty();
		}
	}
	FullCommandOutput.Append(CommandOutput);

	if (ReturnCode == 0)
	{
		FTimecode Timecode;
		bool bFoundTimecode = false;
		FFrameRate FrameRate;
		bool bFoundFrameRate = false;

		FString CommandOutputStr = FString::ConstructFromPtrSize(reinterpret_cast<const UTF8CHAR*>(FullCommandOutput.GetData()), FullCommandOutput.Num());

		// Split the timecode and frame rate lines from the output
		TArray<FString> OutputLinesStr;
		CommandOutputStr.ParseIntoArrayLines(OutputLinesStr);

		for (const FString& Line : OutputLinesStr)
		{
			if (!bFoundFrameRate)
			{
				TArray<FString> FrameRatePartsStr;
				Line.ParseIntoArray(FrameRatePartsStr, TEXT("/"));
				if (FrameRatePartsStr.Num() == 2)
				{
					int32 Numerator = FCString::Atoi(*FrameRatePartsStr[0]);
					int32 Denominator = FCString::Atoi(*FrameRatePartsStr[1]);

					FrameRate = FFrameRate(Numerator, Denominator);
					bFoundFrameRate = true;
					continue;
				}
			}

			if (!bFoundTimecode)
			{
				TArray<FString> TimecodePartsStr;
				Line.ParseIntoArray(TimecodePartsStr, TEXT(":")); // We don't support DF timecode
				if (TimecodePartsStr.Num() == 4)
				{
					int32 Hours = FCString::Atoi(*TimecodePartsStr[0]);
					int32 Minutes = FCString::Atoi(*TimecodePartsStr[1]);
					int32 Seconds = FCString::Atoi(*TimecodePartsStr[2]);
					int32 Frames = FCString::Atoi(*TimecodePartsStr[3]);

					constexpr bool bIsDropFrame = false;
					Timecode = FTimecode(Hours, Minutes, Seconds, Frames, bIsDropFrame);

					bFoundTimecode = true;
					continue;
				}
			}
		}

		if (!bFoundTimecode)
		{
			return MakeError(ECaptureExtractTimecodeError::UnableToParseTimecode);
		}

		if (!bFoundFrameRate)
		{
			return MakeError(ECaptureExtractTimecodeError::UnableToParseTimecodeRate);
		}

		Result = MakeValue(FTimecodeAndRate{ Timecode , FrameRate });
	}
	else
	{
		if (!FullCommandOutput.IsEmpty())
		{
			UE_LOG(LogCaptureExtractTimecode, Display, TEXT("Failed to run the command: %s %s"), *InFFProbePath, *CommandArgs);

			FString CommandOutputStr = FString::ConstructFromPtrSize(reinterpret_cast<const UTF8CHAR*>(FullCommandOutput.GetData()), FullCommandOutput.Num());
			UE_LOG(LogCaptureExtractTimecode, Display,
				TEXT("Output from the command:\n>>>>>>\n%s<<<<<<"), *CommandOutputStr);
		}

		return MakeError(ECaptureExtractTimecodeError::InternalError);
	}

	return Result;
}

FCaptureExtractAudioTimecode::FCaptureExtractAudioTimecode(const FString& InFilePath)
	: FilePath(InFilePath)
{}

UE::CaptureManager::FTimecodeAndRateResult FCaptureExtractAudioTimecode::Extract()
{
	return ExtractTimecodeFromBroadcastWaveFormat(FFrameRate());
}

static FFrameRate EstimateSmpteTimecodeRate(const FFrameRate InMediaFrameRate)
{
	if (FMath::IsNearlyEqual(InMediaFrameRate.AsDecimal(), 60.0))
	{
		return FFrameRate(30'000, 1'000);
	}

	if (FMath::IsNearlyEqual(InMediaFrameRate.AsDecimal(), 59.94))
	{
		// 29.97
		return FFrameRate(30'000, 1'001);
	}

	if (FMath::IsNearlyEqual(InMediaFrameRate.AsDecimal(), 50.0))
	{
		FFrameRate(25'000, 1'000);
	}

	if (FMath::IsNearlyEqual(InMediaFrameRate.AsDecimal(), 48.0))
	{
		FFrameRate(24'000, 1'000);
	}

	return InMediaFrameRate;
}

UE::CaptureManager::FTimecodeAndRateResult FCaptureExtractAudioTimecode::Extract(FFrameRate InFrameRate)
{
	using namespace UE::CaptureManager;

	check(!FilePath.IsEmpty())

	FTimecodeAndRateResult Result = MakeError(ECaptureExtractTimecodeError::UnhandledMedia);

	FString FileExtension = FPaths::GetExtension(FilePath);
	if (FileExtension == "wav")
	{
		// Convert timecode rate to SMPTE timecode rate
		FFrameRate TimecodeRate = EstimateSmpteTimecodeRate(InFrameRate);
		Result = ExtractTimecodeFromBroadcastWaveFormat(TimecodeRate);
	}

	if (Result.HasValue())
	{
		UE_LOG(LogCaptureExtractTimecode, Display, TEXT("Extracted timecode '%s' from audio file: %s"), *Result.GetValue().Timecode.ToString(), *FilePath);
	}
	else
	{
		UE_LOG(LogCaptureExtractTimecode, Display, TEXT("Timecode not found for audio file: %s"), *FilePath);
	}

	return Result;
}

UE::CaptureManager::FTimecodeAndRateResult FCaptureExtractAudioTimecode::ExtractTimecodeFromBroadcastWaveFormat(FFrameRate InTimecodeRate)
{
	using namespace UE::CaptureManager;

	TArray<uint8> WAVData;
	if (FFileHelper::LoadFileToArray(WAVData, *FilePath))
	{
		FWaveModInfo WAVInfo;
		if (WAVInfo.ReadWaveInfo(WAVData.GetData(), WAVData.Num()))
		{
			if (WAVInfo.TimecodeInfo.IsValid())
			{
				FSoundWaveTimecodeInfo TimecodeInfo = *WAVInfo.TimecodeInfo.Get();
				const double NumSecondsSinceMidnight = TimecodeInfo.GetNumSecondsSinceMidnight();

				FFrameRate TimecodeRate = TimecodeInfo.TimecodeRate;
				const bool bTimecodeRateIsSampleRate = TimecodeRate == FFrameRate(TimecodeInfo.NumSamplesPerSecond, 1);
				if (bTimecodeRateIsSampleRate)
				{
					UE_LOG(
						LogCaptureExtractTimecode,
						Display,
						TEXT(
							"Embedded timecode rate is %.2f fps (the sample rate). "
							"This usually indicates there is no timecode rate information in the wav file: %s"
						),
						TimecodeInfo.TimecodeRate.AsDecimal(),
						*FilePath
					);

					if (InTimecodeRate != FFrameRate())
					{
						// Use the provided timecode rate instead
						TimecodeRate = InTimecodeRate;

						UE_LOG(
							LogCaptureExtractTimecode,
							Display,
							TEXT(
								"Taking the embedded audio timecode but estimating an SMPTE audio timecode rate. "
								"Timecode rate for %s set to %.2f"
							),
							*FilePath,
							TimecodeRate.AsDecimal()
						);
					}
				}

				FTimecode AudioTimecode = FTimecode(NumSecondsSinceMidnight, TimecodeRate, TimecodeInfo.bTimecodeIsDropFrame, /* InbRollover = */ true);
				FTimecodeAndRate TimecodeAndRate{ AudioTimecode, TimecodeRate };
				return MakeValue(TimecodeAndRate);
			}
		}
	}

	return MakeError(ECaptureExtractTimecodeError::TimecodeNotFound);
}

#undef LOCTEXT_NAMESPACE
