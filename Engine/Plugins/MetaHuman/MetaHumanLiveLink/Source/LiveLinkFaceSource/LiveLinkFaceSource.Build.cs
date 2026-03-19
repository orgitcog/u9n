// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LiveLinkFaceSource : ModuleRules
{
	public LiveLinkFaceSource(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine"
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"LiveLink",
				"LiveLinkInterface",
				"Slate",
				"SlateCore",
				"InputCore",
				"HTTP",
				"Json", 
				"JsonUtilities",
				"Networking",
				"Sockets",
				"MetaHumanCoreTech",
				"MetaHumanLiveLinkSource",
				"CaptureProtocolStack",
				"CaptureUtils"
			}
			);
	}
}
