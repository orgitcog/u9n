// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNext : ModuleRules
	{
		public AnimNext(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"RigVM",
					"Engine",
					"RenderCore",
					"HierarchyTableRuntime",
					"RewindDebuggerRuntimeInterface"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"UniversalObjectLocator",
					"TraceLog",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"RigVMDeveloper",
						"ToolMenus",
						"Slate",
						"SlateCore",
						"UnrealEd"
					}
				);
			}
		}
	}
}