// Copyright Epic Games, Inc. All Rights Reserved.
using EpicGames.Core;
using UnrealBuildTool;
using System;
using System.IO;

public class ConcurrencyVisualizer : ModuleRules
{
	public ConcurrencyVisualizer(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// We on purpose is not adding any public include paths since we don't want to affect core's rsp/definition files

		bool UseDebug = Target.bDebugBuildsActuallyUseDebugCRT == true && Target.Configuration == UnrealTargetConfiguration.Debug;

		// Defaults to null implementation
		string LibName = UseDebug ? "ConcurrencyVisualizerNullDebug.lib" : "ConcurrencyVisualizerNull.lib";

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			string VisualStudioInstallation = Target.WindowsPlatform.IDEDir;
			if (!string.IsNullOrEmpty(VisualStudioInstallation) && Directory.Exists(VisualStudioInstallation))
			{
				string SubFolderName = @"Common7\IDE\Extensions\rf2nfg00.o0t\SDK\Native\Inc";
				string IncludeDirectory = Path.Combine(VisualStudioInstallation, SubFolderName);

				if (File.Exists(Path.Combine(IncludeDirectory, "cvmarkers.h")))
				{
					// Extension is installed, let's link to full lib
					LibName = UseDebug ? "ConcurrencyVisualizerDebug.lib" : "ConcurrencyVisualizer.lib";
				}
			}
		}

		PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "Lib", Target.Architecture.WindowsLibDir, LibName));

	}
}
