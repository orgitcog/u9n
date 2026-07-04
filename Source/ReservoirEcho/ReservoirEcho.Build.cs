// ReservoirEcho.Build.cs
// Unreal Build System module definition for ReservoirEcho bridge
// Copyright (c) 2026 Deep Tree Echo Project

// SECURITY NOTE: This module intentionally does NOT use PreBuildSteps or
// PostBuildSteps. Any addition of build steps requires explicit security
// review per SECURITY.md policy. See issue #603.

using UnrealBuildTool;

public class ReservoirEcho : ModuleRules
{
    public ReservoirEcho(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeepTreeEcho"
        });

        PublicIncludePaths.AddRange(new string[]
        {
            "ReservoirEcho"
        });
    }
}
