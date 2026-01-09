// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SceneStateGameplayEditor : ModuleRules
{
    public SceneStateGameplayEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "LevelEditor",
                "SceneState",
                "SceneStateBlueprint",
                "SceneStateBlueprintEditor",
                "SceneStateGameplay",
                "Sequencer",
                "SequencerCore",
                "Slate",
                "SlateCore",
                "UnrealEd",
            }
        );
    }
}
