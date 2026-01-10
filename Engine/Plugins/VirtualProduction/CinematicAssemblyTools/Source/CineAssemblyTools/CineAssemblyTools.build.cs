// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CineAssemblyTools : ModuleRules
	{
		public CineAssemblyTools(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"LevelSequence",
					"NamingTokens",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Json",
					"MovieScene",
					"SlateCore",
				}
			);
		}
	}
}
