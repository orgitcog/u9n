// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

public class MaterialX : ModuleRules
{
	public MaterialX(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "MaterialX-1.38.10");

		PublicSystemIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		List<string> MaterialXLibraries = new List<string> {
			"MaterialXCore",
			"MaterialXFormat",
			"MaterialXGenGlsl",
			"MaterialXGenMdl",
			"MaterialXGenMsl",
			"MaterialXGenOsl",
			"MaterialXGenShader",
			"MaterialXRender",
			"MaterialXRenderGlsl",
			"MaterialXRenderHw",
			"MaterialXRenderOsl"
		};

		string LibPostfix = bDebug ? "_d" : "";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.Architecture.WindowsLibDir,
				"lib");

			foreach (string MaterialXLibrary in MaterialXLibraries)
			{
				string StaticLibName = MaterialXLibrary + LibPostfix + ".lib";
				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, StaticLibName));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// MaterialXRenderMsl is only supported on Mac.
			MaterialXLibraries.Add("MaterialXRenderMsl");

			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			foreach (string MaterialXLibrary in MaterialXLibraries)
			{
				string StaticLibName = "lib" + MaterialXLibrary + LibPostfix + ".a";
				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, StaticLibName));
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			// Note that since we no longer support OpenGL on
			// Linux, we do not build the MaterialXRender
			// libraries, since MaterialX does not offer a way to
			// disable only MaterialXRenderGlsl, which requires
			// linking against OpenGL.
			MaterialXLibraries = new List<string> {
				"MaterialXCore",
				"MaterialXFormat",
				"MaterialXGenGlsl",
				"MaterialXGenMdl",
				"MaterialXGenMsl",
				"MaterialXGenOsl",
				"MaterialXGenShader"
			};

			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Unix",
				Target.Architecture.LinuxName,
				"lib");

			foreach (string MaterialXLibrary in MaterialXLibraries)
			{
				string StaticLibName = "lib" + MaterialXLibrary + LibPostfix + ".a";
				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, StaticLibName));
			}
		}

		// Add the MaterialX standard data libraries as runtime dependencies.
		RuntimeDependencies.Add(
			Path.Combine(Target.UEThirdPartyBinariesDirectory, "MaterialX", "libraries", "..."),
			StagedFileType.NonUFS);
	}
}
