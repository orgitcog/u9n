// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AutoRTFMEngineTests : ModuleRules
{
	public AutoRTFMEngineTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");
		PrivateIncludePathModuleNames.Add("DerivedDataCache");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AutomationController",
				"AutomationWorker",
				"Core",
				"Engine",
				"GeometryCore",
				"HeadMountedDisplay",
				"HTTP",
				"InstallBundleManager",
				"MediaUtils",
				"MRMesh",
				"MoviePlayer",
				"MoviePlayerProxy",
				"MovieScene",
				"PreLoadScreen",
				"Projects",
				"SessionServices",
				"SlateNullRenderer",
				"SlateRHIRenderer",
				"ProfileVisualizer",
			}
		);
	}
}
