// Copyright Epic Games, Inc. All Rights Reserved.

using System.Globalization;
using System.IO;
using UnrealBuildBase;
using UnrealBuildTool;

public class DumpSyms : ModuleRules
{
	public DumpSyms(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

		bAddDefaultIncludePaths = false;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string ToolChainDir = Target.WindowsPlatform.ToolChainDir;
			string ToolChainArch = (Target.Architecture == UnrealArch.Arm64) ? "arm64" : "x64";
			PrivateIncludePaths.Add(Path.Combine(ToolChainDir, "atlmfc", "include"));
			PublicAdditionalLibraries.Add(Path.Combine(ToolChainDir, "atlmfc", "lib", ToolChainArch, "atls.lib"));

			string DiaSdkDir = Target.WindowsPlatform.DiaSdkDir;
			string DiaArch = (Target.Architecture == UnrealArch.Arm64) ? "arm64" : "amd64";
			PrivateIncludePaths.Add(Path.Combine(DiaSdkDir, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(DiaSdkDir, "lib", DiaArch, "diaguids.lib"));
			RuntimeDependencies.Add("$(TargetOutputDir)/msdia140.dll", Path.Combine(DiaSdkDir, "bin", DiaArch, "msdia140.dll"));

			string WindowsSdkDir = Target.WindowsPlatform.WindowsSdkDir;
			PublicAdditionalLibraries.Add(Path.Combine(WindowsSdkDir, "Lib", Target.WindowsPlatform.WindowsSdkVersion, "um", "x64", "imagehlp.lib"));
		}

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(EngineDirectory, "Source", "ThirdParty", "Breakpad", "src"),
				Path.Combine(EngineDirectory, "Source", "ThirdParty", "Breakpad", "src", "third_party", "llvm"),
			});

		PrivateDefinitions.Add("_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"mimalloc"
			}
		);

		PrivateDependencyModuleNames.Add("zlib");

		bUseRTTI = true;
	}
}
