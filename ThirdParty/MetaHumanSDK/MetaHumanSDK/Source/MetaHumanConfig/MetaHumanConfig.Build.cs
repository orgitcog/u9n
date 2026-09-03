// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanConfig : ModuleRules
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

	public MetaHumanConfig(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Projects",
			"Slate",
			"SlateCore",
			"PlatformCrypto",
			"PlatformCryptoContext",
			"PlatformCryptoTypes",
			"MetaHumanCaptureData",
			"MeshTrackerInterface",
			"MetaHumanCore",
			"Engine",
			"RigLogicModule",
			"CaptureDataCore"
		});
		
		if (Target.bBuildEditor)
        {
			PrivateDependencyModuleNames.Add("MetaHumanCoreTechLib");
		}
	}
}
