// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextChooser : ModuleRules
	{
		public AnimNextChooser(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"RigVM",
					"ControlRig",
					"Engine",
					"AnimNext",
					"Chooser",
					"AnimNextAnimGraph",
				}
			);
		}
	}
}