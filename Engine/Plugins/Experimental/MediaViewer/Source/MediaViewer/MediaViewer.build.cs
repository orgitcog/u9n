// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MediaViewer : ModuleRules
{
	public MediaViewer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"AppFramework",
				"ApplicationCore",
				"EditorWidgets",
				"Engine",
				"ImageCore",
				"InputCore",
				"LevelEditor",
				"MediaAssets",
				"MediaPlayerEditor",
				"MediaStream",
				"Projects",
				"PropertyEditor",
				"RenderCore",
				"RHI",
				"SlateCore",
				"Slate",
				"ToolWidgets",
				"WorkspaceMenuStructure",
				"UMG",
				"UnrealEd"
			}
		);
	}
}
