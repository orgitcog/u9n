// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MoverMassIntegration : ModuleRules
{
    public MoverMassIntegration(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Mover",
                "MassActors",
                "MassEntity",
                "MassSpawner",
                "MassCommon",
                "MassMovement"
            }
        );
    }
}