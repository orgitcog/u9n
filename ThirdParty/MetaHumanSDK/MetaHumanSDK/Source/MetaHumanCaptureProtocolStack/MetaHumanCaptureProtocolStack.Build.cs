// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanCaptureProtocolStack : ModuleRules
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

	public MetaHumanCaptureProtocolStack(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] 
		{
			// ... add public include paths required here ...
        });


        PrivateIncludePaths.AddRange(new string[] 
		{
			// ... add other private include paths required here ...
		});

        PublicDependencyModuleNames.AddRange(new string[]
		{
            "Core",
			"MetaHumanCaptureUtils"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
            "Json",
            "JsonUtilities",
            "Sockets",
            "Networking",
            "UnrealEd"
        });
	}
}
