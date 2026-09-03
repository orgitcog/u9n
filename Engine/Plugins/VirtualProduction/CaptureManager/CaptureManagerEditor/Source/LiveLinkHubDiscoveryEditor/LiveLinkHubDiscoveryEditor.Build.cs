// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveLinkHubDiscoveryEditor : ModuleRules
{
	public LiveLinkHubDiscoveryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] 
		{
			"CaptureUtils",
			"Core",
			"CoreUObject",
			"LiveLinkHubCaptureMessaging",
			"LiveLinkHubWorkerManager",
			"LiveLinkHubExportServer",
			"Messaging",
			"Sockets",
		});
	}
}
