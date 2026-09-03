// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class PixelStreaming2Editor : ModuleRules
	{
		public PixelStreaming2Editor(ReadOnlyTargetRules Target) : base(Target)
		{
			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"RenderCore",
				"Renderer",
				"RHI",
				"PixelStreaming2RTC",
				"PixelStreaming2Settings",
				"Slate",
				"EngineSettings",
				"InputCore",
				"Json",
				"PixelCapture",
				"PixelStreaming2Servers",
				"HTTP",
				"Sockets",
				"ApplicationCore",
				"PixelStreaming2Input",
				"AVCodecsCore",
				"PixelStreaming2",
				"PixelStreaming2Core"
			});

			PublicDependencyModuleNames.AddRange(new string[] 
			{
				"SlateCore"
			});

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(new string[]
				{
					"UnrealEd",
					"ToolMenus",
					"EditorStyle",
					"DesktopPlatform",
					"LevelEditor",
					"MainFrame"
				});
			}

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Apple))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "MetalCPP");
			}
		}
	}
}
