// ReservoirEcho.Build.cs
// Unreal Build System module definition for ReservoirEcho bridge
// Copyright (c) 2026 Deep Tree Echo Project

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
