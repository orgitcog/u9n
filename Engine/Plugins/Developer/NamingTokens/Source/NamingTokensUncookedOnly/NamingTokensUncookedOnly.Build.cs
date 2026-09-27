// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NamingTokensUncookedOnly : ModuleRules
{
    public NamingTokensUncookedOnly(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "NamingTokens"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
	            "AssetDefinition",
	            "BlueprintGraph",
                "CoreUObject",
                "Engine",
                "EngineAssetDefinitions",
                "InputCore",
				"Projects",
                "Slate",
                "SlateCore",
                "UnrealEd",
            }
        );
    }
}