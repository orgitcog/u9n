// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SceneStateMachineEditor : ModuleRules
{
    public SceneStateMachineEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "SceneStateMachineGraph",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "GraphEditor",
                "Projects",
                "PropertyEditor",
                "SceneState",
                "SceneStateTransitionGraph",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd",
            }
        );
    }
}
