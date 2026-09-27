// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SpatialReadiness : ModuleRules
{
    public SpatialReadiness(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Engine",
				"CoreUObject",
            });

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
            }
        );

		SetupModulePhysicsSupport(Target);
    }
}
