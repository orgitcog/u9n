// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LiveLinkCapabilities : ModuleRules
{
	public LiveLinkCapabilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		ShortName = "LiveLinkCapabilities";

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"CaptureUtils",
			"Core",
			"CaptureManagerUnrealEndpoint" // TODO: Only needed for the ULiveLinkHubIngestClientInfo definition, move it and remove this dependency
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CaptureManagerMediaRW",
			"Json",
			"LiveLinkDevice",
			"Media",
			"LiveLinkHubCaptureMessaging",
			"SlateCore",
			"Slate",
			"CaptureManagerTakeMetadata",
			"CoreUObject"
		});

		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
