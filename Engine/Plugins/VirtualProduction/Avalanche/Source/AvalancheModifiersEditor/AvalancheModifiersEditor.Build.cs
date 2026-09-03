// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheModifiersEditor : ModuleRules
{
	public AvalancheModifiersEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AvalancheModifiers",
				"AvalancheOutliner",
				"Core",
				"SlateCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ActorModifier",
				"ActorModifierCore",
				"ActorModifierCoreEditor",
				"ActorModifierEditor",
				"CoreUObject",
				"Engine",
				"Projects",
				"Slate",
				"ToolMenus",
				"UnrealEd",
			}
		);
	}
}
