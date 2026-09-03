// AngelClaw.Build.cs
// Unreal Build System module definition for AngelClaw integration
// Copyright (c) 2026 Deep Tree Echo Project

using UnrealBuildTool;

public class AngelClaw : ModuleRules
{
    public AngelClaw(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeepTreeEcho",
            "Neurochemical",
            "ReservoirEcho"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AIModule",
            "GameplayTasks"
        });

        PublicIncludePaths.AddRange(new string[]
        {
            "AngelClaw"
        });
    }
}
