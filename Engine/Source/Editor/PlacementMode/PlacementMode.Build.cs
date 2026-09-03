// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PlacementMode : ModuleRules
{
    public PlacementMode(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateIncludePathModuleNames.Add("AssetTools");

        PublicDependencyModuleNames.AddRange( 
            new string[] { 
                "Engine", 
            } 
        );
        
        PrivateDependencyModuleNames.AddRange( 
            new string[] { 
                "Core", 
                "CoreUObject",
                "InputCore",
                "Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
                "ContentBrowser",
				"ContentBrowserData",
                "CollectionManager",
                "LevelEditor",
                "AssetTools",
                "EditorWidgets",
                "ToolMenus",
                "WidgetRegistration"
            } 
        );
    }
}
