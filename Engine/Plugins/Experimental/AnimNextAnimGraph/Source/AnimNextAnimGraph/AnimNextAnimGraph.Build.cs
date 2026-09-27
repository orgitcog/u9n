// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextAnimGraph : ModuleRules
	{
		public AnimNextAnimGraph(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"RigVM",
					"Engine",
					"AnimNext",
					"HierarchyTableRuntime",
					"HierarchyTableAnimationRuntime",
					"TraceLog",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"AssetRegistry",
						"RigVMDeveloper",
					}
				);
			}
		}
	}
}