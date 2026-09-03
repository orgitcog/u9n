// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DataHierarchyEditor : ModuleRules
{
    public DataHierarchyEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", 
                "CoreUObject",
                "EditorWidgets",
                "SlateCore"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Engine",
                "Slate",
                "UnrealEd",
                "ToolMenus",
                "InputCore"
            }
        );
    }
}