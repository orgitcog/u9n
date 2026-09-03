// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanCaptureSource : ModuleRules
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

	public MetaHumanCaptureSource(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"ImgMedia",
			"Engine",
			"MetaHumanCaptureUtils",
			"CaptureDataCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AudioEditor",
			"UnrealEd",
			"DesktopWidgets",
			"Slate",
			"SlateCore",
			"Json",
			"AssetDefinition",
			"CameraCalibrationCore",
			"OodleDataCompression",
			"MetaHumanCaptureData",
			"MetaHumanCore",
			"MetaHumanCoreEditor",
			"MetaHumanPipelineCore",
			"MetaHumanPipeline",
			"MetaHumanCaptureProtocolStack",
			"ImageWriteQueue",
			"RenderCore",
			"NNE",
			"MetaHumanFaceContourTracker",
			"InputCore",
			"CaptureDataUtils"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			//  Windows Media Foundation dependencies for the iPhone take data conversion
			//	Footage retrieval only works on Windows platforms for now.
			PublicDelayLoadDLLs.AddRange(new[] {
				"mf.dll", "mfplat.dll", "mfreadwrite.dll", "propsys.dll"
			});

			PublicSystemLibraries.AddRange(new[]
			{
				"mf.lib", "mfplat.lib", "mfreadwrite.lib", "mfuuid.lib", "propsys.lib"
			});
		}

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows)
			|| Target.Platform == UnrealTargetPlatform.Mac
			|| Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicDefinitions.Add("WITH_LIBJPEGTURBO=1");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "LibJpegTurbo");
		}
	}
}
