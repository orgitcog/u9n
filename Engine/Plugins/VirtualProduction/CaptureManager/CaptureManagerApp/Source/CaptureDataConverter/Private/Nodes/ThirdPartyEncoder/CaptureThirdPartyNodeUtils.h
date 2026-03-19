// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

namespace UE::CaptureManager
{

FString WrapInQuotes(const FString& InString);
TArray<uint8> ReadPipe(void* InReadPipe);

}
