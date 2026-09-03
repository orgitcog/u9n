// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TP_TopDown : ModuleRules
{
	public TP_TopDown(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"TP_TopDown",
			"TP_TopDown/Variant_Strategy",
			"TP_TopDown/Variant_Strategy/UI",
			"TP_TopDown/Variant_TwinStick",
			"TP_TopDown/Variant_TwinStick/AI",
			"TP_TopDown/Variant_TwinStick/Gameplay",
			"TP_TopDown/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
