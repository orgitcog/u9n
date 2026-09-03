// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Logging/LogMacros.h"

// Log AudioTiming. For verbose detailed timings diagnostics and debugging.
#if UE_BUILD_SHIPPING
	// Dial back the compiled verbosity in shipping
	#define AUDIO_TIMING_COMPILED_VERBOSITY Error
#else // UE_BUILD_SHIPPING
	#define AUDIO_TIMING_COMPILED_VERBOSITY All
#endif // UE_BUILD_SHIPPING

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogAudioTiming, Log, AUDIO_TIMING_COMPILED_VERBOSITY);
