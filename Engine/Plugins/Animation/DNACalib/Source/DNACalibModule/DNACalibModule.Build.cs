// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DNACalibModule : ModuleRules
	{
		public DNACalibModule(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"RigLogicModule",
				} 
			);

			if (Target.Type == TargetType.Editor)
			{
				PublicDependencyModuleNames.Add("UnrealEd");
			}

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DNACalibLib",
					"RigLogicLib"
				}
			 );
		}
	}
}