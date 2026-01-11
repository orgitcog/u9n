// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LiveLinkHubCaptureMessaging : ModuleRules
{
	public LiveLinkHubCaptureMessaging(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CaptureUtils",
			"CaptureProtocolStack"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"MessagingCommon"
		});
	}
}
