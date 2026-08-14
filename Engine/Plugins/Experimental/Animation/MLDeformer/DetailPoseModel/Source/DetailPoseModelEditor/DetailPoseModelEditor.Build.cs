// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DetailPoseModelEditor : ModuleRules
	{
		public DetailPoseModelEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"InputCore",
					"UnrealEd",
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					"Projects",
					"MLDeformerFramework",
					"MLDeformerFrameworkEditor",
					"DetailPoseModel",
					"NeuralMorphModel",
					"NeuralMorphModelEditor",
					"PropertyEditor",
					"ToolWidgets",
					"DeveloperSettings",
					"GeometryCache"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
				}
			);
		}
	}
}
