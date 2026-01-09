// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextWarping : ModuleRules
	{
		public AnimNextWarping(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"AnimationCore",
					"RigVM",
					"Engine",
					"AnimNext",
					"AnimNextAnimGraph",
				}
			);
		}
	}
}