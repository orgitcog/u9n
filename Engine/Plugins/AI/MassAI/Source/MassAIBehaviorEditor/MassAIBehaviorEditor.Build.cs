// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MassAIBehaviorEditor : ModuleRules
	{
		public MassAIBehaviorEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
			new string[] {
			}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AIModule",
				"AppFramework",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"PropertyEditor",
				"MassAIBehavior",
				"MassEntity",
				"Slate",
				"SlateCore",
			}
			);
		}

	}
}
