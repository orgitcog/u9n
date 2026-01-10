// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureThirdPartyNodeUtils.h"

#include "HAL/PlatformProcess.h"

namespace UE::CaptureManager
{

FString WrapInQuotes(const FString& InString)
{
	if (InString.StartsWith(TEXT("\"")) && InString.EndsWith(TEXT("\"")))
	{
		return InString;
	}

	return FString::Format(TEXT("\"{0}\""), { InString });
}

TArray<uint8> ReadPipe(void* InReadPipe)
{
	TArray<uint8> CommandOutput;
	bool Read = FPlatformProcess::ReadPipeToArray(InReadPipe, CommandOutput);
	if (!Read)
	{
		CommandOutput.Empty();
	}

	return CommandOutput;
}

}
