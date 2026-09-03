// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SceneStateBlueprintEditor : ModuleRules
{
    public SceneStateBlueprintEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "SceneStateEditor",
                "UnrealEd",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AssetDefinition",
                "BlueprintGraph",
                "DetailCustomizations",
                "Engine",
                "GraphEditor",
                "InputCore",
                "Kismet",
                "KismetCompiler",
                "PropertyBindingUtils",
                "PropertyBindingUtilsEditor",
                "PropertyEditor",
                "SceneState",
                "SceneStateBinding",
                "SceneStateBlueprint",
                "SceneStateEvent",
                "SceneStateMachineEditor",
                "SceneStateMachineGraph",
                "SceneStateTransitionGraph",
                "Slate",
                "SlateCore",
                "StructUtilsEditor",
                "ToolMenus",
                "ToolWidgets",
            }
        );
    }
}
