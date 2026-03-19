// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class StereoVideoIngestDevice : ModuleRules
{
	public StereoVideoIngestDevice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		ShortName = "StereoVideoIngest";

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"CaptureUtils",
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Engine",
			"Json",
			"CaptureManagerMediaRW",
			"IngestLiveLinkDevice",
			"DataIngestCore",
			"LiveLinkDevice",
			"LiveLinkCapabilities",
			"Media",
			"LiveLinkHubCaptureMessaging",
			"LiveLinkHub",
			"ToolWidgets",
			"Slate",
			"SlateCore",
			"CaptureManagerTakeMetadata",
			"CaptureManagerSettings",
			"CoreUObject",
			"UnrealEd",
			"VideoLiveLinkDeviceCommon"
		});

		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
