// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextControlRigEditor : ModuleRules
	{
		public AnimNextControlRigEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"Core",
					"ControlRig",
					"ControlRigDeveloper",
					"RigVM",
					"AnimNext",
					"AnimNextAnimGraph",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"RigVMDeveloper",
					"WorkspaceEditor",
					"SlateCore",
					"Slate",
					"RigVMEditor",
					"AnimNextEditor",
					"AnimNextControlRig",
					"AnimNextUncookedOnly",
					"AnimNextAnimGraphUncookedOnly",
				}
			);
		}
	}
}
