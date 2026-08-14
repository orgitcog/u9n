// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class LiveLinkFaceSourceEditor : ModuleRules
{
	public LiveLinkFaceSourceEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
		});
			
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"LiveLinkInterface",
			"Slate",
			"SlateCore",
			"InputCore",
			"Sockets",
			"LiveLinkFaceSource",
			"LiveLinkFaceDiscovery",
			"CaptureProtocolStack",
			"CaptureUtils"
		});
	}
}
