// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CaptureDataConverter : ModuleRules
{
	public CaptureDataConverter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Engine",
			"CaptureUtils",
			"CaptureManagerTakeMetadata",
			"CaptureManagerPipeline",
			"CaptureManagerMediaRW"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ContentBrowserData",
			"CoreUObject",
			"ImageWrapper",
			"Json",
			"DataIngestCore",
			"Media",
			"CaptureManagerSettings",
			"NamingTokens",
			"UnrealEd"
		});
	}
}
