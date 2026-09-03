// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureConvertAudioDataThirdParty.h"

#include "MediaSample.h"

#include "Async/HelperFunctions.h"
#include "Async/StopToken.h"
#include "Async/TaskProgress.h"

#include "Nodes/CaptureCopyProgressReporter.h"
#include "CaptureThirdPartyNodeUtils.h"

#include "CaptureManagerMediaRWModule.h"

#include "Engine/Engine.h"
#include "GlobalNamingTokens.h"
#include "NamingTokenData.h"
#include "NamingTokensEngineSubsystem.h"
#include "Settings/CaptureManagerSettings.h"
#include "Settings/CaptureManagerTemplateTokens.h"

#define LOCTEXT_NAMESPACE "CaptureConvertAudioDataTP"

DEFINE_LOG_CATEGORY_STATIC(LogCaptureConvertAudioDataThirdParty, Log, All);

FCaptureConvertAudioDataThirdParty::FCaptureConvertAudioDataThirdParty(FCaptureThirdPartyNodeParams InThirdPartyEncoder,
																	   const FTakeMetadata::FAudio& InAudio,
																	   const FString& InOutputDirectory,
																	   const FCaptureConvertDataNodeParams& InParams,
																	   const FCaptureConvertAudioOutputParams& InAudioParams)
	: FConvertAudioNode(InAudio, InOutputDirectory)
	, ThirdPartyEncoder(MoveTemp(InThirdPartyEncoder))
	, Params(InParams)
	, AudioParams(InAudioParams)
{
}

FCaptureConvertAudioDataThirdParty::FResult FCaptureConvertAudioDataThirdParty::Run()
{
	using namespace UE::CaptureManager;

	if (Params.StopToken.IsStopRequested())
	{
		FText Message = LOCTEXT("CaptureConvertAudioNodeTP_AbortedByUser", "Aborted by user");
		return MakeError(MoveTemp(Message));
	}

	if (FPaths::GetExtension(Audio.Path) == AudioParams.Format)
	{
		return CopyAudioFile();
	}

	return ConvertAudioFile();
}

FCaptureConvertAudioDataThirdParty::FResult FCaptureConvertAudioDataThirdParty::CopyAudioFile()
{
	using namespace UE::CaptureManager;

	FTaskProgress::FTask Task = Params.TaskProgress->StartTask();

	FString DestinationDirectory = OutputDirectory / Audio.Name;
	const FString AudioFilePath = FPaths::ConvertRelativePathToFull(Params.TakeOriginDirectory, Audio.Path);

	FCopyProgressReporter ProgressReporter(Task, Params.StopToken);
	FString Destination = DestinationDirectory / FPaths::SetExtension(AudioParams.AudioFileName, AudioParams.Format);

	constexpr bool bReplace = true;
	constexpr bool bEvenIfReadOnly = true;
	constexpr bool bAttributes = false;
	uint32 Result = IFileManager::Get().Copy(*Destination, *AudioFilePath, bReplace, bEvenIfReadOnly, bAttributes, &ProgressReporter);

	if (Result == COPY_Fail)
	{
		FText Message = FText::Format(LOCTEXT("CaptureConvertAudioNodeTP_CopyFailed", "Failed to copy the audio file {0}"), 
									  FText::FromString(AudioFilePath));
		return MakeError(MoveTemp(Message));
	}

	if (Result == COPY_Canceled)
	{
		FText Message = LOCTEXT("CaptureConvertAudioNodeTP_AbortedByUser", "Aborted by user");
		return MakeError(MoveTemp(Message));
	}

	return MakeValue();
}

FCaptureConvertAudioDataThirdParty::FResult FCaptureConvertAudioDataThirdParty::ConvertAudioFile()
{
	using namespace UE::CaptureManager;

	FTaskProgress::FTask Task = Params.TaskProgress->StartTask();

	FString DestinationDirectory = OutputDirectory / Audio.Name;
	const FString AudioFilePath = FPaths::ConvertRelativePathToFull(Params.TakeOriginDirectory, Audio.Path);

	const FString AudioOutputFile = FPaths::SetExtension(FPaths::Combine(DestinationDirectory, AudioParams.AudioFileName), AudioParams.Format);

	if (ThirdPartyEncoder.CommandArguments.IsEmpty())
	{
		ThirdPartyEncoder.CommandArguments = UE::CaptureManager::AudioCommandArgumentTemplate;
	}

	FString CommandArgs = ThirdPartyEncoder.CommandArguments;

	using namespace UE::CaptureManager;

	UNamingTokensEngineSubsystem* NamingTokensSubsystem = GEngine->GetEngineSubsystem<UNamingTokensEngineSubsystem>();
	check(NamingTokensSubsystem);

	const UCaptureManagerSettings* Settings = GetDefault<UCaptureManagerSettings>();

	FNamingTokenFilterArgs AudioEncoderTokenArgs;
	const TObjectPtr<const UCaptureManagerAudioEncoderTokens> Tokens = Settings->GetAudioEncoderNamingTokens();
	check(Tokens);
	AudioEncoderTokenArgs.AdditionalNamespacesToInclude.Add(Tokens->GetNamespace());
	AudioEncoderTokenArgs.bNativeOnly = true;

	FStringFormatNamedArguments AudioEncoderFormatArgs;
	AudioEncoderFormatArgs.Add(Tokens->GetToken(FString(AudioEncoderTokens::InputKey)).Name, WrapInQuotes(AudioFilePath));
	AudioEncoderFormatArgs.Add(Tokens->GetToken(FString(AudioEncoderTokens::OutputKey)).Name, WrapInQuotes(AudioOutputFile));

	CommandArgs = FString::Format(*CommandArgs, AudioEncoderFormatArgs);
	FNamingTokenResultData AudioEncoderCommandResult = NamingTokensSubsystem->EvaluateTokenString(CommandArgs, AudioEncoderTokenArgs);
	CommandArgs = AudioEncoderCommandResult.EvaluatedText.ToString();

	UE_LOG(LogCaptureConvertAudioDataThirdParty, Display, TEXT("Running the command: %s %s"), *ThirdPartyEncoder.Encoder, *CommandArgs);

	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	verify(FPlatformProcess::CreatePipe(ReadPipe, WritePipe, false));

	constexpr bool bLaunchDetached = false;
	constexpr bool bLaunchHidden = true;
	constexpr bool bLaunchReallyHidden = true;
	FProcHandle ProcHandle = 
		FPlatformProcess::CreateProc(*ThirdPartyEncoder.Encoder, *CommandArgs, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, nullptr, 0, nullptr, WritePipe, nullptr);

	ON_SCOPE_EXIT
	{
		if (Params.StopToken.IsStopRequested())
		{
			FPlatformProcess::TerminateProc(ProcHandle);
		}

		FPlatformProcess::CloseProc(ProcHandle);
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
	};

	if (!ProcHandle.IsValid())
	{
		FText Message = FText::Format(LOCTEXT("CaptureConvertAudioNodeTP_ProcessNotFound", "Failed to start the process {0} {1}"),
									  FText::FromString(ThirdPartyEncoder.Encoder),
									  FText::FromString(CommandArgs));
		return MakeError(MoveTemp(Message));
	}

	TArray<uint8> FullCommandOutput;
	while (FPlatformProcess::IsProcRunning(ProcHandle))
	{
		TArray<uint8> CommandOutput = UE::CaptureManager::ReadPipe(ReadPipe);

		if (CommandOutput.IsEmpty())
		{
			FPlatformProcess::Sleep(0.1);
		}

		FullCommandOutput.Append(MoveTemp(CommandOutput));

		if (Params.StopToken.IsStopRequested())
		{
			FText Message = LOCTEXT("CaptureConvertAudioNodeTP_AbortedByUser", "Aborted by user");
			return MakeError(MoveTemp(Message));
		}
	}

	int32 ReturnCode = 0;
	FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);

	FullCommandOutput.Append(UE::CaptureManager::ReadPipe(ReadPipe));
	if (ReturnCode != 0)
	{
		if (!FullCommandOutput.IsEmpty())
		{
			UE_LOG(LogCaptureConvertAudioDataThirdParty, Error,
				   TEXT("Failed to run the command: %s %s"), *ThirdPartyEncoder.Encoder, *CommandArgs);

			FString CommandOutputStr = FString::ConstructFromPtrSize(reinterpret_cast<const UTF8CHAR*>(FullCommandOutput.GetData()), FullCommandOutput.Num());
			UE_LOG(LogCaptureConvertAudioDataThirdParty, Display,
				   TEXT("Output from the command:\n>>>>>>\n%s<<<<<<"), *CommandOutputStr);
		}

		FText Message = FText::Format(LOCTEXT("CaptureConvertAudioNodeTP_ErrorRunning", "Error while running the third party encoder (ReturnCode={0})"), 
									  FText::AsNumber(ReturnCode));
		return MakeError(MoveTemp(Message));
	}

	Task.Update(1.0f);
	return MakeValue();
}

#undef LOCTEXT_NAMESPACE