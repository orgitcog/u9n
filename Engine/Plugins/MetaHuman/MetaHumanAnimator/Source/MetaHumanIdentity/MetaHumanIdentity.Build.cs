// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanIdentity : ModuleRules
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

	public MetaHumanIdentity(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"SlateCore",
			"GeometryFramework",
			"MetaHumanCore",
			"MetaHumanCaptureData",
			"MetaHumanPipelineCore",
			"MetaHumanCoreTechLib"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"ImgMedia",
			"GeometryCore",
			"RigLogicModule",
			"MeshConversion",
			"MeshDescription",
			"SkeletalMeshDescription",
			"StaticMeshDescription",
			"Projects",
			"Json",
			"JsonUtilities",
			"MetaHumanFaceContourTracker",
			"MetaHumanFaceFittingSolver",
			"MetaHumanFaceAnimationSolver",
			"MetaHumanConfig",
			"MetaHumanPipeline",
			"CameraCalibrationCore",
			"DNAInterchange",
			"CaptureDataCore",
			"MetaHumanCoreTech"
		});

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("SkeletalMeshUtilitiesCommon");
			PrivateDependencyModuleNames.Add("Slate");
			PrivateDependencyModuleNames.Add("ControlRigDeveloper");
			PrivateDependencyModuleNames.Add("MetaHumanCaptureDataEditor");
			PublicDependencyModuleNames.Add("MetaHumanSDKEditor");
		}
	}
}