// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextControlRig : ModuleRules
	{
		public AnimNextControlRig(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"Core",
					"ControlRig",
					"RigVM",
					"AnimNext",
					"AnimNextAnimGraph",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
				   new string[]
				   {
					   "ControlRigDeveloper",
						"WorkspaceEditor",
						"SlateCore",
						"Slate",
						"AnimNextUncookedOnly",
						"RigVMDeveloper",
						"AnimationCore",
				   });
			}
		}
	}
}
