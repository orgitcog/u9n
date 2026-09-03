// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"

namespace UE::PixelStreaming2
{
	PIXELSTREAMING2SERVERS_API extern int GetNextAvailablePort(TOptional<int> StartingPort = FNullOpt(0));
} // namespace UE::PixelStreaming2
