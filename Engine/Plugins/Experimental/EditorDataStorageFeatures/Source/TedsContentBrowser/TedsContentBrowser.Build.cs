// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TedsContentBrowser : ModuleRules
{
	public TedsContentBrowser(ReadOnlyTargetRules Target) : base(Target)
	{
		// Enable truncation warnings in this module
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (Target.bBuildEditor)
		{
			PublicIncludePaths.AddRange(new string[] {});
			PrivateIncludePaths.AddRange(new string[] {});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject"
				});
			
			PrivateDependencyModuleNames.AddRange(
            	new string[]
            	{
		            "TypedElementFramework",
		            "SlateCore",
		            "Slate",
		            "TedsTableViewer",
		            "ContentBrowser",
		            "ContentBrowserData",
		            "InputCore",
					"TedsAlerts",
		            "TedsAssetData",
		            "ToolWidgets",
		            "TedsQueryStack"
            	});
			
			DynamicallyLoadedModuleNames.AddRange(new string[] {});
		}
	}
}
