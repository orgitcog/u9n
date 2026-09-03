// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ExampleLiveLinkDevices : ModuleRules
{
	public ExampleLiveLinkDevices(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"CaptureUtils",
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"LiveLinkDevice",
			"IngestLiveLinkDevice",
			"LiveLinkCapabilities",
			"CaptureManagerTakeMetadata",
			"CoreUObject",
		});

		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
