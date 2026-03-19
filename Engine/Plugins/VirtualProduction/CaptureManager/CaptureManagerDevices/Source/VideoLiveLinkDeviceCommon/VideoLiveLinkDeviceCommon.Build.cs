// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class VideoLiveLinkDeviceCommon : ModuleRules
{
	public VideoLiveLinkDeviceCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		ShortName = "VideoLLDCommon";

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"ImageCore",
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CaptureManagerMediaRW",
			"Engine",
			"Slate",
			"SlateCore",
			"CoreUObject",
			"UnrealEd"
		});

		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
