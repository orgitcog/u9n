// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LiveLinkOpenTrackIO : ModuleRules
{
	public LiveLinkOpenTrackIO(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"LiveLinkInterface",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"Cbor",
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities",
				"LiveLinkLens",
				"Projects",
				"Networking",
				"Sockets",
				"Slate",
				"SlateCore",
				"Serialization"
			}
			);
	}
}
