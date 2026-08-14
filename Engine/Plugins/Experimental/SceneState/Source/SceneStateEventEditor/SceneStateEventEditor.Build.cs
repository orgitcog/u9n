// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SceneStateEventEditor : ModuleRules
{
    public SceneStateEventEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AssetDefinition",
                "AssetTools",
                "BlueprintGraph",
                "Core",
                "CoreUObject",
                "DetailCustomizations",
                "Engine",
                "GraphEditor",
                "InputCore",
                "KismetWidgets",
                "PropertyEditor",
                "SceneStateEvent",
                "Slate",
                "SlateCore",
                "StructUtilsEditor",
                "UnrealEd",
            }
        );
    }
}