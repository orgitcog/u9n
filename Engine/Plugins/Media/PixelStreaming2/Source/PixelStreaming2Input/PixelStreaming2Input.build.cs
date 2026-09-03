// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PixelStreaming2Input : ModuleRules
	{
		public PixelStreaming2Input(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Json",
					"SlateCore",
					"Slate",
					"DeveloperSettings",
					"HeadMountedDisplay",
					"PixelStreaming2Settings"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
					"Core",
					"InputCore",
					"InputDevice",
					"XRBase",
					"PixelStreaming2HMD"
				}
			);
		}
	}
}