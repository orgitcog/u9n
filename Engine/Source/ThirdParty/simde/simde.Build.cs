// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class simde : ModuleRules
{
    public simde(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
		
		PublicSystemIncludePaths.Add(ModuleDirectory);
        PublicSystemIncludePaths.Add(Path.Join(ModuleDirectory, "include"));

		bDisableStaticAnalysis = true;
	}
}
