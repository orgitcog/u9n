// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheDataLinkEditor : ModuleRules
{
    public AvalancheDataLinkEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AvalancheDataLink",
                "Core",
                "CoreUObject",
                "Engine",
                "PropertyEditor",
                "Slate",
                "SlateCore", 
            }
        );
    }
}
