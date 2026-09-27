// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildBase;
using UnrealBuildTool;

public class PsdSDK : ModuleRules
{
	public virtual string LibName
	{
		get
		{
			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Microsoft))
			{
				return "Psd.lib";
			}

			return "Psd.a";
		}
	}

	public PsdSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		bool bIsDebugConfig = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string ConfigName = bIsDebugConfig ? "Debug" : "Release";
		
		PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "Libraries", Target.Platform.ToString(), ConfigName, LibName));
		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Includes"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PublicDefinitions.Add("WITH_PSD");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// TODO : Add supported libraries for Mac

			PublicDefinitions.Add("WITH_PSD");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			// TODO : Add supported libraries for Linux

			PublicDefinitions.Add("WITH_PSD");
		}
	}
}
