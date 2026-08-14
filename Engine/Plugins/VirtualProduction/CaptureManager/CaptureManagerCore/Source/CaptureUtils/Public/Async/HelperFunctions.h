// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Function.h"

namespace UE::CaptureManager
{

// Sends InFunction to be called on the GameThread and waits for the result
void CAPTUREUTILS_API CallOnGameThread(TFunction<void()> InFunction);

}