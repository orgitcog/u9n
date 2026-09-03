// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class IngestLiveLinkDevice : ModuleRules
{
	public IngestLiveLinkDevice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		ShortName = "IngestLiveLinkDevice";

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"CaptureUtils",
			"Core",
			"MediaAssets"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CaptureProtocolStack",
			"CaptureDataConverter",
			"CaptureManagerUnrealEndpoint",
			"Engine",
			"Json",
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
			"ElectraPlayerPlugin"
		});

		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
