// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChaosMover : ModuleRules
	{
		public ChaosMover(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"GameplayTags",
					"InputCore",
					"NetCore",
					"Mover",
					"Water"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"DeveloperSettings",
					"Engine"
				}
			);

			SetupModulePhysicsSupport(Target);
		}
	}
}