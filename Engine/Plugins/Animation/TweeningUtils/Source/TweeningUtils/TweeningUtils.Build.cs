// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TweeningUtils : ModuleRules
{
    public TweeningUtils(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Engine"
            }
        );
    }
}