// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosVDBuiltInExtensions : ModuleRules
{
    public ChaosVDBuiltInExtensions(ReadOnlyTargetRules Target) : base(Target)
    {
	    PublicDependencyModuleNames.AddRange(
		    new string[]
		    {
			    "ChaosVD",
		    });

	    PrivateDependencyModuleNames.AddRange(
            new string[]
            {
	            "Core",
                "CoreUObject",
                "Engine",
                "ChaosVDData",
                "ChaosVDRuntime",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd",
                "EditorWidgets",
                "TraceServices"
            }
        );
        
        SetupModulePhysicsSupport(Target);
    }
}