// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"

#include "Containers/UnrealString.h"

namespace UE::CaptureManager
{

/** Parse timecode from string. */
DATAINGESTCORE_API FTimecode ParseTimecode(const FString& InTimecodeString);

/** Parse frame rate from double. */
DATAINGESTCORE_API FFrameRate ParseFrameRate(double InFrameRate);

}