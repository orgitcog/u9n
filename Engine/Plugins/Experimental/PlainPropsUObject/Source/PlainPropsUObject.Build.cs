// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PlainPropsUObject : ModuleRules
{
	public PlainPropsUObject(ReadOnlyTargetRules Target) : base(Target)
	{
		bDisableStaticAnalysis = true;

		CppStandard = CppStandardVersion.Cpp20;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"CorePreciseFP",
				"Engine",
				"PlainProps"
		});
	}
}
