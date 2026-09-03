// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualCameraEditor : ModuleRules
{
	public VirtualCameraEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V2;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"CinematicCamera",
				"EditorWidgets",
				"Engine",
				"MovieScene",
				"MovieSceneTracks",
				"PlacementMode", 
				"Settings",
				"UnrealEd",
				"TakeRecorderSources",
				"VCamCore",
				"VirtualCamera", 
				"VPUtilitiesEditor", 
			}
		);
	}
}
