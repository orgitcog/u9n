// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanCaptureData : ModuleRules
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

	public MetaHumanCaptureData(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CaptureDataCore",
			"ImgMedia"
		});

		if (Target.Type == TargetType.Editor)
		{
			// TODO: This is required for the capture data to create a UFootageComponent
			// ideally the UFootageComponent would be able to be created in a runtime environment
			// but it currently depends on editor only functions from CustomMaterialUtils
			PrivateDependencyModuleNames.Add("MetaHumanImageViewer");
			PrivateDependencyModuleNames.Add("DirectoryWatcher");
		}
	}
}