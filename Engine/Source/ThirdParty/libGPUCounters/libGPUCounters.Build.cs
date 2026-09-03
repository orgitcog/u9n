// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libGPUCounters : ModuleRules
{
	public libGPUCounters(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		string BasePath = Target.UEThirdPartySourceDirectory + "libGPUCounters/";
		PublicSystemIncludePaths.Add(BasePath + "Public");

		if (Target.Platform == UnrealTargetPlatform.Android && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PublicAdditionalLibraries.Add(BasePath + "lib/arm64-v8a/libGPUCounters.so");

			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "libGPUCounters_UPL.xml"));
		}
	}
}
