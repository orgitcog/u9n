// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class UElibPNG : ModuleRules
{
	// no longer needed, once all subclasses remove overrides, delete
	protected virtual string LibRootDirectory { get { return ""; } }
	protected virtual string IncRootDirectory { get { return ""; } }

	protected virtual string LibPNGVersion
	{
		get
		{
        	// On Windows x64, use the LLVM compiled version with changes made by us to improve performance
			// due to better vectorization and FMV support that will take advantage of the different instruction
			// sets depending on CPU supported features.
			// Please, take care of bringing those changes over if you upgrade the library
			return "libPNG-1.6.44";
		}
	}

	protected virtual string IncPNGPath { get { return Path.Combine(ModuleDirectory, LibPNGVersion); } }
	protected virtual string LibPNGPath { get { return Path.Combine(PlatformModuleDirectory, LibPNGVersion, "lib"); } }

	public UElibPNG(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_LIBPNG_1_6=" + (LibPNGVersion.Contains("-1.6.") ? "1" : "0"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibConfig = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";
            PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, "Win64", Target.Architecture.WindowsLibDir, LibConfig, "libpng.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string TargetConfig = Target.Configuration == UnrealTargetConfiguration.Debug ? "Debug" : "Release";
			PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, "Mac", TargetConfig, "libpng.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.IOS))
		{
			string LibDir = (Target.Architecture == UnrealArch.IOSSimulator || Target.Architecture == UnrealArch.TVOSSimulator)
				? "Simulator"
				: "Device";
			PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, PlatformSubdirectoryName, LibDir, "libpng.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, "Android", "arm64", "libpng.a"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, "Android", "x64", "libpng.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string TargetConfig = Target.Configuration == UnrealTargetConfiguration.Debug ? "Debug" : "Release";
			PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, "Unix", Target.Architecture.LinuxName, TargetConfig, "libpng.a"));
		}

		PublicSystemIncludePaths.Add(IncPNGPath);
	}
}
