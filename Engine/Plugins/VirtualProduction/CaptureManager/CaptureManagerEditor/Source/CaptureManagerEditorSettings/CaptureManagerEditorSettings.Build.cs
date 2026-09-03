// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using UnrealBuildTool.Rules;

public class CaptureManagerEditorSettings : ModuleRules
{
	public CaptureManagerEditorSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"CaptureUtils"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Engine",
			"DeveloperSettings",
			"CaptureManagerStyle",
			"ContentBrowserData",
			"LiveLinkHubCaptureMessaging",
			"LiveLinkHubExportServer",
			"LiveLinkHubMessaging",
			"LiveLinkInterface",
			"NamingTokens",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"Settings",
			"ToolMenus",
			"UnrealEd"
		});
	}
}
