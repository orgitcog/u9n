// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TP_FirstPerson : ModuleRules
{
	public TP_FirstPerson(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"TP_FirstPerson",
			"TP_FirstPerson/Variant_Horror",
			"TP_FirstPerson/Variant_Horror/UI",
			"TP_FirstPerson/Variant_Shooter",
			"TP_FirstPerson/Variant_Shooter/AI",
			"TP_FirstPerson/Variant_Shooter/UI",
			"TP_FirstPerson/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
