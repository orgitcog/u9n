// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class DatasmithSketchUpRuby2025Target : DatasmithSketchUpRubyBaseTarget
{
	public DatasmithSketchUpRuby2025Target(TargetInfo Target)
		: base(Target)
	{
		LaunchModuleName = "DatasmithSketchUpRuby2025";
		ExeBinariesSubFolder = @"SketchUpRuby/2025";

		AddCopyPostBuildStep(Target);
	}
}
