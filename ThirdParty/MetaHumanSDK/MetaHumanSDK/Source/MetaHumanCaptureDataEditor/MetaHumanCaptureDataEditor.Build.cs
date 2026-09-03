// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanCaptureDataEditor : ModuleRules
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

	public MetaHumanCaptureDataEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"MetaHumanCoreEditor",
			"CaptureDataCore",
			"CaptureDataEditor",
			"SlateCore",
			"Slate",
			"InputCore",
			"Engine"
		});

		if (Target.Type == TargetType.Editor)
		{
			// TODO: This is required for the capture data to create a UFootageComponent
			// ideally the UFootageComponent would be able to be created in a runtime environment
			// but it currently depends on editor only functions from CustomMaterialUtils
			PrivateDependencyModuleNames.Add("MetaHumanImageViewerEditor");
		}
	}
}