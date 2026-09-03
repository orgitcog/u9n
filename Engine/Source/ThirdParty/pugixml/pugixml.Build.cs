// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class pugixml : ModuleRules
{
	public pugixml(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "pugixml-1.15");

		PublicSystemIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		string LibPostfix = bDebug ? "_d" : "";

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.Architecture.WindowsLibDir,
				"lib");

			string StaticLibName = "pugixml" + LibPostfix + ".lib";

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, StaticLibName));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			string StaticLibName = "libpugixml" + LibPostfix + ".a";

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, StaticLibName));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Unix",
				Target.Architecture.LinuxName,
				"lib");

			string StaticLibName = "libpugixml" + LibPostfix + ".a";

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, StaticLibName));
		}
	}
}
