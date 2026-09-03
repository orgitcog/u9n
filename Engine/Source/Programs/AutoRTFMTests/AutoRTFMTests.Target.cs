// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;
using System.Collections.Generic;
using UnrealBuildBase;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class AutoRTFMTestsTarget : TargetRules
{
	// TODO: Might be useful to promote this to a general Target.cs setting at some point in the future.
	[CommandLine("-AllowLogFile")]
	public bool bAllowLogFile = false;

	[CommandLine("-NoVirtualStackAllocator")]
	public bool bForceNoVirtualStackAllocator = false;

	public AutoRTFMTestsTarget(TargetInfo Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		LaunchModuleName = "AutoRTFMTests";

		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;

		// Logs are still useful to print the results
		bUseLoggingInShipping = true;

		// Make a console application under Windows, so entry point is main() everywhere
		bIsBuildingConsoleApplication = true;

		// Disable unity builds by default for AutoRTFMTest
		bUseUnityBuild = false;

		// Disable ICU to match the server config
		bCompileICU = false;

		// Set the RTFM clang compiler
		if (!bGenerateProjectFiles)
		{
			 bUseAutoRTFMCompiler = true;
		}

		// Match FortniteServer FName settings.
		bFNameOutlineNumber = true;

		// Match FortniteServer VirtualStackAllocator settings.
		if (!bForceNoVirtualStackAllocator) 
		{
			GlobalDefinitions.Add("UE_USE_VIRTUAL_STACK_ALLOCATOR_FOR_SCRIPT_VM=1");
		}

		MinCpuArchX64 = MinimumCpuArchitectureX64.AVX;

		bCompileWithStatsWithoutEngine = true;
		GlobalDefinitions.Add("ENABLE_STATNAMEDEVENTS=1");
		GlobalDefinitions.Add("ENABLE_STATNAMEDEVENTS_UOBJECT=1");

		// Allow for disabling writing out the logfile, since in `PreSubmitTest.py` we run this target simultaneously
		// multiple times, and doing so would cause writing them out to stomp each other.
		if (!bAllowLogFile)
		{
			GlobalDefinitions.Add("ALLOW_LOG_FILE=0");
		}
		else
		{
			GlobalDefinitions.Add("ALLOW_LOG_FILE=1");
		}

		GlobalDefinitions.Add("MALLOC_LEAKDETECTION=1");
		GlobalDefinitions.Add("PLATFORM_USES_FIXED_GMalloc_CLASS=0");

		GlobalDefinitions.Add("UNIX_PLATFORM_FILE_SPEEDUP_FILE_OPERATIONS=1");

		// Disable crashreporter to improve startup time.
		GlobalDefinitions.Add("NOINITCRASHREPORTER=1");

		// Disable AutoRTFM runtime by default when compiler support is enabled
		GlobalDefinitions.Add("UE_AUTORTFM_ENABLED_RUNTIME_BY_DEFAULT=0");

		// CsvProfiler tests rely on these being enabled!
		GlobalDefinitions.Add("CSV_PROFILER=1");
		GlobalDefinitions.Add("CSV_PROFILER_ENABLE_IN_SHIPPING=1");

		// Need to force the heartbeat on so we can test with it!
		GlobalDefinitions.Add("USE_HITCH_DETECTION=1");

		// For testing insights, this sets `UE_TRACE_ENABLED`.
		bEnableTrace = true;
	}
}
