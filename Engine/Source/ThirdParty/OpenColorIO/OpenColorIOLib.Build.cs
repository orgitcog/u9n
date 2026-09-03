// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class OpenColorIOLib : ModuleRules
{
	private string ProjectBinariesDir
	{
		get
		{
			return "$(TargetOutputDir)";
		}
	}

	public OpenColorIOLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bIsPlatformAdded = false;

		string PlatformDir = Target.Platform.ToString();
		string DeployDir = Path.Combine(ModuleDirectory, "Deploy", "OpenColorIO");

		PublicSystemIncludePaths.Add(Path.Combine(DeployDir, "include"));
		
		AddEngineThirdPartyPrivateStaticDependencies(Target, "Imath");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string Arch = Target.Architecture.WindowsLibDir;

			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, Arch, "libexpatMD.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, Arch, "libminizip-ng.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, Arch, "pystring.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, Arch, "yaml-cpp.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, Arch, "OpenColorIO.lib"));

			bIsPlatformAdded = true;
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string Arch = Target.Architecture.LinuxName;
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", "Unix", Arch, "libexpat.a"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", "Unix", Arch, "libminizip-ng.a"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", "Unix", Arch, "libpystring.a"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", "Unix", Arch, "libyaml-cpp.a"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", "Unix", Arch, "libOpenColorIO.a"));

			bIsPlatformAdded = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, "libexpat.a"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, "libminizip-ng.a"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, "libpystring.a"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, "libyaml-cpp.a"));
			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, "libOpenColorIO.a"));

			bIsPlatformAdded = true;
		}

		// @note: Any update to this definition should be mirrored in the wrapper module's WITH_OCIO.
		PublicDefinitions.Add("WITH_OCIO_LIB=" + (bIsPlatformAdded ? "1" : "0"));
	}
}
