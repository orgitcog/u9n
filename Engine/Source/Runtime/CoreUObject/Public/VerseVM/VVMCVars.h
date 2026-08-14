// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/IConsoleManager.h"

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "Math/RandomStream.h"
#endif // WITH_VERSE_VM

namespace Verse
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarTraceExecution;
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarSingleStepTraceExecution;
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarDumpBytecode;
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarDumpBytecodeAsCFG;
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarDoBytecodeRegisterAllocation;
extern COREUOBJECT_API TAutoConsoleVariable<float> CVarUObjectProbability;
extern COREUOBJECT_API FRandomStream RandomUObjectProbability;
#endif // WITH_VERSE_VM

extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarUseDynamicSubobjectInstancing;
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarEnableAssetClassRedirectors;
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarForceCompileFramework;
} // namespace Verse
