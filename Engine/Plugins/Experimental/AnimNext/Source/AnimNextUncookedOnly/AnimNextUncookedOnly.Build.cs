// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextUncookedOnly : ModuleRules
	{
		public AnimNextUncookedOnly(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"RigVM",
					"RigVMDeveloper",
					"AnimNext",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"BlueprintGraph",	// For K2 schema
					"AnimationCore",
					"AnimGraph",
					"Kismet",
					"Slate",
					"SlateCore",
					"StructUtilsEditor",
					"ToolMenus",
					"UniversalObjectLocator", 
					"UniversalObjectLocatorEditor",
					"WorkspaceEditor",
					"MessageLog", 
					"AdvancedWidgets",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd",
						"SourceControl",
					}
				);

				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"WorkspaceEditor",
					}
				);
				
				PrivateIncludePathModuleNames.AddRange(
					new string[]
					{
						"AnimNextEditor",
					}
				);

				DynamicallyLoadedModuleNames.AddRange(
					new string[]
					{
						"AnimNextEditor",
					}
				);
			}
		}
	}
}