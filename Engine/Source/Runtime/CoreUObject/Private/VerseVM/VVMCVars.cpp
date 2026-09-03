// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMCVars.h"

namespace Verse
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)

TAutoConsoleVariable<bool> CVarTraceExecution(TEXT("verse.TraceExecution"), false, TEXT("When true, print a trace of Verse instructions executed to the log."), ECVF_Default);
TAutoConsoleVariable<bool> CVarSingleStepTraceExecution(TEXT("verse.SingleStepTraceExecution"), false, TEXT("When true, require input from stdin before continuing to the next bytecode."), ECVF_Default);
TAutoConsoleVariable<bool> CVarDumpBytecode(TEXT("verse.DumpBytecode"), false, TEXT("When true, dump bytecode of all functions."), ECVF_Default);
TAutoConsoleVariable<bool> CVarDumpBytecodeAsCFG(TEXT("verse.DumpBytecodeAsCFG"), false, TEXT("When true, bytecode dumps as a CFG with liveness info.\n"), ECVF_Default);
TAutoConsoleVariable<bool> CVarDoBytecodeRegisterAllocation(TEXT("verse.DoBytecodeRegisterAllocation"), false, TEXT("When true, run register allocation over bytecode.\n"), ECVF_Default);
TAutoConsoleVariable<float> CVarUObjectProbability(TEXT("verse.UObjectProbablity"), 0.0f, TEXT("Probability (0.0..1.0) that we substitute VObjects with UObjects upon creation (for testing only)."), ECVF_Default);
FRandomStream RandomUObjectProbability{42}; // Constant seed so sequence is deterministic

#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)

TAutoConsoleVariable<bool> CVarUseDynamicSubobjectInstancing(TEXT("verse.UseDynamicSubobjectInstancing"), true, TEXT("If true, Verse classes compiled with dynamic reference support will use dynamic subobject instancing."), ECVF_Default);
TAutoConsoleVariable<bool> CVarEnableAssetClassRedirectors(TEXT("verse.EnableAssetClassRedirectors"), false, TEXT("If true, asset class aliases will be added to the list of redirectors for their package."), ECVF_Default);
/** Fallback to legacy behaviour */
TAutoConsoleVariable<bool> CVarEditInlineSubobjectProperties(TEXT("verse.EditInlineSubobjectProperties"), true, TEXT("If true, ObjectProperties of Verse classes will all be tagged with EditInline."), ECVF_Default);

TAutoConsoleVariable<bool> CVarForceCompileFramework(TEXT("verse.ForceCompileFramework"), WITH_VERSE_VM, TEXT("When true, compile framework packages from source instead of loading cooked packages."), ECVF_Default);

} // namespace Verse
