// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TechAudioToolsMetaSoundEditor : ModuleRules
{
    public TechAudioToolsMetaSoundEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
	            "Blutility",
                "Core",
				"MetasoundEditor",
				"MetasoundEngine",
				"MetasoundFrontend",
				"ModelViewViewModel",
				"ModelViewViewModelEditor",
				"TechAudioToolsMetaSound",
				"UnrealEd",
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
