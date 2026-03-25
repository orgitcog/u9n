// EchoSelf.Build.cs
// Unreal Build System module definition for EchoSelf integration
// Copyright (c) 2026 Deep Tree Echo Project

using UnrealBuildTool;

public class EchoSelf : ModuleRules
{
    public EchoSelf(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeepTreeEcho"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AngelClaw",
            "ReservoirEcho"
        });

        PublicIncludePaths.AddRange(new string[]
        {
            "EchoSelf"
        });
    }
}
