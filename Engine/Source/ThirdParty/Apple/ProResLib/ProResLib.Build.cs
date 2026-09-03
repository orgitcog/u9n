// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class ProResLib : ModuleRules
{
	public ProResLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		string IncPath = Path.Combine(ModuleDirectory, "include");
		PublicSystemIncludePaths.Add(IncPath);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibPath = Path.Combine(ModuleDirectory, "lib", "windows");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "ProRes64_VS2017.lib"));
		}
	}
}
