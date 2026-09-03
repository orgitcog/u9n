// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EditorInteractiveToolsFramework : ModuleRules
{
	public EditorInteractiveToolsFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		// Enable truncation warnings in this module
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"TypedElementFramework",
				// ... add other public dependencies that you statically link with here ...
			}
			);			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"RenderCore",
				"Slate",
                "SlateCore",
                "InputCore",
				"EditorFramework",
				"UnrealEd",
                "ContentBrowser",
                "LevelEditor",
                "ApplicationCore",
                "InteractiveToolsFramework",
				"MeshDescription",
				"StaticMeshDescription",
                "EditorSubsystem",
                "TypedElementRuntime",
                "AnimationCore",
                "ViewportSnapping", // For ISnappingPolicy support
                // ... add private dependencies that you statically link with here ...
			}
            );
	}
}
