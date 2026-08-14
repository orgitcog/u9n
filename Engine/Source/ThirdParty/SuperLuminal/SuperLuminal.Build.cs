// Copyright Epic Games, Inc. All Rights Reserved.
using EpicGames.Core;
using UnrealBuildTool;
using System;
using System.IO;

public class SuperLuminal : ModuleRules
{
	public SuperLuminal(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string SuperLuminalFolder = "Null";

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			string SuperluminalInstallDir = OperatingSystem.IsWindows() ? Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Superluminal\Performance", "InstallDir", null) as string : null;
			if (String.IsNullOrEmpty(SuperluminalInstallDir))
			{
				SuperluminalInstallDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Superluminal", "Performance");
			}

			string SuperluminalApiDir = Path.Combine(SuperluminalInstallDir, "API");
			string SuperluminalLibDir = Path.Combine(SuperluminalApiDir, "lib", Target.Architecture.WindowsLibDir);

			if (Target.Configuration != UnrealTargetConfiguration.Shipping &&
				File.Exists(Path.Combine(SuperluminalApiDir, "include/Superluminal/PerformanceAPI_capi.h")))
			{
				ExtraRootPath = ("SuperLuminal", SuperluminalApiDir);

				if (Target.bDebugBuildsActuallyUseDebugCRT == true && Target.Configuration == UnrealTargetConfiguration.Debug)
				{
					PublicAdditionalLibraries.Add(Path.Combine(SuperluminalLibDir, "PerformanceAPI_MDd.lib"));
					SuperLuminalFolder = "MDd";
				}
				else
				{
					PublicAdditionalLibraries.Add(Path.Combine(SuperluminalLibDir, "PerformanceAPI_MD.lib"));
					SuperLuminalFolder = "MD";
				}
				PrivateDefinitions.Add("WITH_SUPERLUMINAL_PROFILER=1");
				PrivateIncludePaths.Add(Path.Combine(SuperluminalApiDir, "include/"));
			}
			else
			{
				PrivateDefinitions.Add("WITH_SUPERLUMINAL_PROFILER=0");
			}
		}

		PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "Lib", Target.Architecture.WindowsLibDir, SuperLuminalFolder, "SuperLuminal.lib"));
	}
}
