// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanFootageIngest : ModuleRules
{
	protected bool BuildForDevelopment
	{
		get
		{
			// Check if source is available
			string SourceFilesPath = Path.Combine(ModuleDirectory, "Private");
			return Directory.Exists(SourceFilesPath) &&
				   Directory.GetFiles(SourceFilesPath).Length > 0;
		}
	}

	public MetaHumanFootageIngest(ReadOnlyTargetRules Target) : base(Target)
	{
		bool bIsUEFN = ((Target.Name == "UnrealEditorFortnite") || (Target.Name == "FortniteContentWorker"));

		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"ContentBrowserData",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"InputCore",
			"MetaHumanCore",
			"ToolMenus",
			"ToolWidgets",
			"EditorStyle",
			"Projects",
			"PropertyEditor",
			"ImgMedia",
			"Json",
			"Sockets",
			"Networking",
			"GeometryCore",
			"GeometryFramework",
			"MeshDescription",
			"StaticMeshDescription",
			"MetaHumanCaptureSource",
			"MetaHumanCaptureData",
			"MetaHumanCaptureUtils",
			"MetaHumanCoreEditor",
			"CaptureDataCore",
			"MeshTrackerInterface",
			"WorkspaceMenuStructure",
			"CaptureDataUtils",
			"LauncherPlatform",
			"LiveLinkHubEditor"
		});

		PrivateDefinitions.Add("HIDE_MAIN_MENU");

		if (!bIsUEFN)
		{
			PrivateDefinitions.Add("SHOW_CAPTURE_SOURCE_FILTER");
		}
	}
}