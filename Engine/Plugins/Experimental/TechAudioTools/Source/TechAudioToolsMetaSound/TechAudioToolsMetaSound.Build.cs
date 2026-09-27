// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TechAudioToolsMetaSound : ModuleRules
{
    public TechAudioToolsMetaSound(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
				"MetasoundEngine",
				"MetasoundFrontend",
				"ModelViewViewModel",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
            }
        );
    }
}
