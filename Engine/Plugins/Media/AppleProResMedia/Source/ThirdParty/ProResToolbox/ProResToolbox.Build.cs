// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class ProResToolbox : ModuleRules
{
	public ProResToolbox(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		string IncPath = Path.Combine(ModuleDirectory, "include");
		PublicSystemIncludePaths.Add(IncPath);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibPath = Path.Combine(ModuleDirectory, "lib");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "ProResToolbox.lib"));

			string LibraryName = "ProResToolbox";
			PublicDelayLoadDLLs.Add(LibraryName + ".dll");

			RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/Win64/ProResToolbox.dll");
		}
	}
}
