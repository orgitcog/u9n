using UnrealBuildTool;
using System;
using System.IO;

public class Flatbuffers : ModuleRules
{
    public Flatbuffers(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Includes
		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "flatbuffers-24.3.25", "include"));
    }
}
