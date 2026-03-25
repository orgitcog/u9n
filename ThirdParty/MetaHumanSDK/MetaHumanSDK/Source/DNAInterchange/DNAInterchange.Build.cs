// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DNAInterchange : ModuleRules
{
	public DNAInterchange(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"InterchangeCore",
				"InterchangeCommon",
				"InterchangeEngine",
				"InterchangePipelines",
				"InterchangeImport",
				"InterchangeNodes",
				"LevelSequence",
				"MeshDescription",
				"StaticMeshDescription",
				"SkeletalMeshDescription",
				"RigLogicLib",
				
			}
		);	
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"RigLogicModule",
				"Projects",
			}
		);
	}
}
