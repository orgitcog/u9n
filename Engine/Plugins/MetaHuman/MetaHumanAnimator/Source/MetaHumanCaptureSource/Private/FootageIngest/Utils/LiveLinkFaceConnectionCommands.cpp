// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/LiveLinkFaceConnectionCommands.h"

const FString FStartCaptureCommandArgs::CommandName = TEXT("StartCapture");

FStartCaptureCommandArgs::FStartCaptureCommandArgs(FString InSlateName,
												   uint16 InTakeNumber,
												   TOptional<FString> InSubject,
												   TOptional<FString> InScenario,
												   TOptional<TArray<FString>> InTags)
	: FBaseCommandArgs(CommandName)
	, SlateName(MoveTemp(InSlateName))
	, TakeNumber(InTakeNumber)
	, Subject(MoveTemp(InSubject))
	, Scenario(MoveTemp(InScenario))
	, Tags(MoveTemp(InTags))
{
}

const FString FStopCaptureCommandArgs::CommandName = TEXT("StopCapture");

FStopCaptureCommandArgs::FStopCaptureCommandArgs(bool bInShouldFetchTake)
	: FBaseCommandArgs(CommandName)
	, bShouldFetchTake(bInShouldFetchTake)
{
}