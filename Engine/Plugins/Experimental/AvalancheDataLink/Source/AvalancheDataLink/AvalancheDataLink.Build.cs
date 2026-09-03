// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheDataLink : ModuleRules
{
    public AvalancheDataLink(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "AvalancheRemoteControl",
                "Core",
                "CoreUObject",
                "DataLink",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Avalanche",
                "DataLinkJson",
                "Engine",
                "Json",
                "JsonUtilities",
                "PropertyBindingUtils",
                "RemoteControl",
                "RemoteControlLogic",
            }
        );
    }
}
