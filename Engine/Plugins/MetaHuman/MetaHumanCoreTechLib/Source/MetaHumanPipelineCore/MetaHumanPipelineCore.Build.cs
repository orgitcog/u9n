// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetaHumanPipelineCore : ModuleRules
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

	public MetaHumanPipelineCore(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Eigen",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"EventLoop",
			"ModelingOperators",
			"NNE",
			"AudioPlatformConfiguration",
			"CaptureDataCore",
			"MetaHumanCoreTech",
			"MetaHumanCaptureData",
		});

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
