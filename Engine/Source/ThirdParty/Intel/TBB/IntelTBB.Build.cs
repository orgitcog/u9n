// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

public class IntelTBB : ModuleRules
{
	public IntelTBB(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "oneTBB-2021.13.0");

		PublicSystemIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		string LibPostfix = bDebug ? "_debug" : "";

		if (bDebug)
		{
			PublicDefinitions.Add("TBB_USE_DEBUG=1");
		}

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string PlatformDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.Architecture.WindowsLibDir);

			string BinDirectory = Path.Combine(PlatformDirectory, "bin");
			string LibDirectory = Path.Combine(PlatformDirectory, "lib");

			PublicSystemLibraryPaths.Add(LibDirectory);

			List<string> TBBLibraries = new List<string> {
				"tbb12",
				"tbbmalloc"
			};

			foreach (string TBBLibrary in TBBLibraries)
			{
				string TBBDllName = TBBLibrary + LibPostfix + ".dll";
				string TBBLibName = TBBLibrary + LibPostfix + ".lib";

				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, TBBLibName));

				RuntimeDependencies.Add(
					Path.Combine("$(TargetOutputDir)", TBBDllName),
					Path.Combine(BinDirectory, TBBDllName));

				if (bDebug)
				{
					string TBBPdbName = TBBLibrary + LibPostfix + ".pdb";

					RuntimeDependencies.Add(
						Path.Combine("$(TargetOutputDir)", TBBPdbName),
						Path.Combine(BinDirectory, TBBPdbName),
						StagedFileType.DebugNonUFS);
				}
			}

			// Disable the #pragma comment(lib, ...) used by default in tbb & tbbmalloc.
			// We want to explicitly include the libraries.
			PublicDefinitions.Add("__TBB_NO_IMPLICIT_LINKAGE=1");
			PublicDefinitions.Add("__TBBMALLOC_NO_IMPLICIT_LINKAGE=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			List<string> TBBLibraries = new List<string> {
				"tbb" + LibPostfix,
				"tbb" + LibPostfix + ".12",
				"tbb" + LibPostfix + ".12.13",
				"tbbmalloc" + LibPostfix,
				"tbbmalloc" + LibPostfix + ".2",
				"tbbmalloc" + LibPostfix + ".2.13"
			};

			foreach (string TBBLibrary in TBBLibraries)
			{
				string TBBLibName = "lib" + TBBLibrary + ".dylib";

				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, TBBLibName));

				RuntimeDependencies.Add(
					Path.Combine("$(TargetOutputDir)", TBBLibName),
					Path.Combine(LibDirectory, TBBLibName));
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			bUseRTTI = false;
			bEnableExceptions = false;
			PublicDefinitions.Add("TBB_USE_EXCEPTIONS=0");

			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Unix",
				Target.Architecture.LinuxName,
				"lib");

			List<string> TBBLibraries = new List<string> {
				"tbb" + LibPostfix + ".so",
				"tbb" + LibPostfix + ".so.12",
				"tbb" + LibPostfix + ".so.12.13",
				"tbbmalloc" + LibPostfix + ".so",
				"tbbmalloc" + LibPostfix + ".so.2",
				"tbbmalloc" + LibPostfix + ".so.2.13"
			};

			foreach (string TBBLibrary in TBBLibraries)
			{
				string TBBLibName = "lib" + TBBLibrary;

				// The shared library file names with a version suffix
				// confuse the extension stripping that UBT does, causing it
				// to pass extra malformed '-l' options to the linker. We only
				// need to include one anyway, so only add the non-suffixed
				// ".so" as an additional library.
				if (TBBLibName.EndsWith(".so"))
				{
					PublicAdditionalLibraries.Add(
						Path.Combine(LibDirectory, TBBLibName));
				}

				RuntimeDependencies.Add(
					Path.Combine("$(TargetOutputDir)", TBBLibName),
					Path.Combine(LibDirectory, TBBLibName));
			}
		}
	}
}
