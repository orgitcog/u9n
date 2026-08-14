// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanSequencer : ModuleRules
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

	public MetaHumanSequencer(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"Engine",
			"MovieScene",
			"MediaCompositing",
			"MovieSceneTools",
			"MovieSceneTracks",
			"Sequencer",
			"CaptureDataCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject",
			"UnrealEd",
			"ModelingOperators",
			"GeometryCore",
			"Slate",
			"SlateCore",
			"MediaCompositingEditor",
			"MediaAssets",
			"ImgMedia",
			"ToolMenus",
			"Persona",
			"ControlRig",
			"MetaHumanCaptureData",
			"MetaHumanCore",
		});
	}
}
