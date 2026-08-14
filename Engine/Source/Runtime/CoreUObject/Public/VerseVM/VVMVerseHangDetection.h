// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace VerseHangDetection
{

COREUOBJECT_API float VerseHangThreshold();
COREUOBJECT_API bool IsComputationLimitExceeded(const double StartTime, double HangThreshold = VerseHangThreshold());

} // namespace VerseHangDetection