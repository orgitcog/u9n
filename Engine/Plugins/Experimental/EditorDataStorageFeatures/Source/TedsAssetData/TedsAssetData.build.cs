// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// Experimental test module. please refrain from depending on it until this warning is removed
public class TedsAssetData : ModuleRules
{
	public TedsAssetData(ReadOnlyTargetRules Target) : base(Target)
	{
		// Enable truncation warnings in this module
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;

		ShortName = "TEDSAssetD";

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (Target.bBuildEditor)
		{
			PublicIncludePaths.AddRange(new string[] { });
			PrivateIncludePaths.AddRange(new string[] { });

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"AssetDefinition",
					"AssetRegistry",
					"ContentBrowserData",
					"Core",
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					"TedsAlerts",
					"ToolWidgets",
					"TypedElementFramework",
					"UnrealEd",
					"UnrealEd",
					"AssetDefinition",
				});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Projects",
					"ContentBrowser"
				});

		}
	}
}
