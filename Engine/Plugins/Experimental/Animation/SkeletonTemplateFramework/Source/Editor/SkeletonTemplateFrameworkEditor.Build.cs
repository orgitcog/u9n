// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;

public class SkeletonTemplateFrameworkEditor : ModuleRules
{
	public SkeletonTemplateFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "STFEd";

		PrivateDependencyModuleNames.AddAll(
			"AssetDefinition",
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UnrealEd",
			"SkeletonTemplateFrameworkRuntime",
			"Slate",
			"SlateCore"
		);
	}
}
