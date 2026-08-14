// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

public class Boost : ModuleRules
{
	public Boost(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "boost-1.85.0");

		PublicSystemIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		List<string> BoostLibraries = new List<string> {
			"atomic",
			"chrono",
			"filesystem",
			"iostreams",
			"program_options",
			"regex",
			"system",
			"thread"
		};

		if (Target.Platform == UnrealTargetPlatform.Mac || (!Target.Architectures.bIsMultiArch && Target.Architecture.bIsX64))
		{
			// We have universal binaries for Python on Mac, but currently only x64
			// builds of Python for Windows and Linux, so we restrict boost::python
			// to those platforms/architectures.
			BoostLibraries.Add("python311");
		}

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.Architecture.WindowsLibDir,
				"lib");

			string LibArchPostfix = "x64";
			if (Target.Architecture == UnrealArch.Arm64)
			{
				LibArchPostfix = "a64";
			}

			foreach (string BoostLibrary in BoostLibraries)
			{
				string BoostLibName = "boost_" + BoostLibrary + "-mt-" + LibArchPostfix;
				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, BoostLibName + ".lib"));	
				RuntimeDependencies.Add(
					Path.Combine("$(TargetOutputDir)", BoostLibName + ".dll"),
					Path.Combine(LibDirectory, BoostLibName + ".dll"));
			}

			PublicDefinitions.Add("BOOST_ALL_NO_LIB");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			foreach (string BoostLibrary in BoostLibraries)
			{
				// Note that these file names identify the universal binaries
				// that support both x86_64 and arm64.
				string BoostLibName = "libboost_" + BoostLibrary + "-mt-a64";
				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, BoostLibName + ".a"));
				RuntimeDependencies.Add(
					Path.Combine("$(TargetOutputDir)", BoostLibName + ".dylib"),
					Path.Combine(LibDirectory, BoostLibName + ".dylib"));
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Unix",
				Target.Architecture.LinuxName,
				"lib");

			string LibArchPostfix = "x64";
			if (!Target.Architecture.bIsX64)
			{
				LibArchPostfix = "a64";
			}

			foreach (string BoostLibrary in BoostLibraries)
			{
				string BoostLibName = "libboost_" + BoostLibrary + "-mt-" + LibArchPostfix;
				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, BoostLibName + ".a"));

				// Declare all version variations of the shared libraries as
				// runtime dependencies.
				foreach (string BoostSharedLibPath in Directory.EnumerateFiles(LibDirectory, BoostLibName + ".so*"))
				{
					RuntimeDependencies.Add(
						Path.Combine("$(TargetOutputDir)", Path.GetFileName(BoostSharedLibPath)),
						BoostSharedLibPath);
				}
			}
		}
	}
}
