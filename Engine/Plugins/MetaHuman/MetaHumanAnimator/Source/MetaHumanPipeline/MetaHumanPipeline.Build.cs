// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanPipeline : ModuleRules
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

	public MetaHumanPipeline(ReadOnlyTargetRules Target) : base(Target)
	{
		bool bUseOpenCV = false;

		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"Eigen",
			"MetaHumanCore",
			"MetaHumanSpeech2Face",
			"MetaHumanCoreTech",
			"MetaHumanCoreTechLib",
			"MetaHumanPipelineCore",
			"MeshTrackerInterface",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject",
			"ModelingOperators",
			"NNE",
			"Engine",
			"Json",
			"AudioPlatformConfiguration",
			"MetaHumanCaptureData",
			"GeometryCore",
			"Projects",
			"RenderCore",
			"MetaHumanConfig",
			"MetaHumanFaceAnimationSolver",
			"MetaHumanPlatform",
			"CaptureDataCore"
		});

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		if (bUseOpenCV)
		{
			PrivateDefinitions.Add("USE_OPENCV");
			PrivateDependencyModuleNames.Add("OpenCVHelper");
			PrivateDependencyModuleNames.Add("OpenCV");
		}
	}
}
