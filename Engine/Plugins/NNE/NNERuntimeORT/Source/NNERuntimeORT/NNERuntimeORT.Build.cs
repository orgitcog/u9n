// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class NNERuntimeORT : ModuleRules
{
	public NNERuntimeORT( ReadOnlyTargetRules Target ) : base( Target )
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"NNE",
			"NNEOnnxruntime",
			"Projects",
			"RenderCore",
			"DeveloperSettings",
			"RHI"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"D3D12RHI"
			});

			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"DirectML",
				"DX12"
			});

			if (Version.TryParse(Target.WindowsPlatform.WindowsSdkVersion, out Version WindowsSdkVersion) && WindowsSdkVersion.Build >= 26100 /* Windows 11, version 24H2 */)
			{
				PublicDefinitions.Add("ORT_USE_NEW_DXCORE_FEATURES");
			}
		}

		if (Target.bBuildEditor)
		{
			bEnableExceptions = true;
			bDisableAutoRTFMInstrumentation = true;
		}
	}
}
