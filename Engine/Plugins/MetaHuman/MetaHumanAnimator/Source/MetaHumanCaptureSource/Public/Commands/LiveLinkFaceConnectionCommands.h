// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCommand.h"
#include "Misc/Optional.h"

class METAHUMANCAPTURESOURCE_API FStartCaptureCommandArgs
	: public FBaseCommandArgs
{
public:
	static const FString CommandName;

	FStartCaptureCommandArgs(FString InSlateName,
							 uint16 InTakeNumber,
							 TOptional<FString> InSubject = TOptional<FString>(),
							 TOptional<FString> InScenario = TOptional<FString>(),
							 TOptional<TArray<FString>> InTags = TOptional<TArray<FString>>());

	virtual ~FStartCaptureCommandArgs() override = default;

	FString SlateName;
	uint16 TakeNumber;
	TOptional<FString> Subject;
	TOptional<FString> Scenario;
	TOptional<TArray<FString>> Tags;
};

class METAHUMANCAPTURESOURCE_API FStopCaptureCommandArgs
	: public FBaseCommandArgs
{
public:
	static const FString CommandName;

	FStopCaptureCommandArgs(bool bInShouldFetchTake = true);

	virtual ~FStopCaptureCommandArgs() override = default;

	// Introduced as a solution to the current async code and object lifecycle.
	// Should be removed when a proper design is in place.
	bool bShouldFetchTake;
};
