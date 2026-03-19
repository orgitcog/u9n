// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextStateTree : ModuleRules
	{
		public AnimNextStateTree(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimNextAnimGraph",
					"RigVM"
				});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"StateTreeModule",
					"Engine",
					"AnimNext",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"AnimNextAnimGraph",
						"AnimNextAnimGraphUncookedOnly",
						"AnimNextEditor",
						"AnimNextUncookedOnly",
						"RigVMDeveloper",
						"StateTreeEditorModule",
						"UnrealEd"
					});
			}
		}
	}
}