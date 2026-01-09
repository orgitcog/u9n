// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SceneStateTransitionGraph : ModuleRules
{
    public SceneStateTransitionGraph(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "BlueprintGraph",
                "Core",
                "CoreUObject",
                "Engine",
                "SceneState",
            }
        );

        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "GraphEditor",
                    "UnrealEd",
                });
        }
    }
}
