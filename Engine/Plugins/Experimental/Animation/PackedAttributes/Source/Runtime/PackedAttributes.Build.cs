// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PackedAttributes : ModuleRules
{
	public PackedAttributes(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
