// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DataIngestCoreEditor : ModuleRules
{
	public DataIngestCoreEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Engine",
			"CaptureUtils",
			"CaptureDataCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"UnrealEd",
			"Media",
			"ImgMedia",
			"CameraCalibrationCore",
			"MeshDescription",
			"StaticMeshDescription",
			"AssetDefinition",
			"Json",
			"JsonUtilities",
			"SlateCore",
			"Slate",
			"InputCore",
			"PropertyEditor",
			"DataIngestCore",
			"CaptureDataEditor",
			"CaptureDataCore",
			"CaptureManagerEditorSettings",
			"NamingTokens"
		});
	}
}
