// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNERuntimeCoreML : ModuleRules
{
	public NNERuntimeCoreML( ReadOnlyTargetRules Target ) : base( Target )
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"NNE",
			"Projects"
		});

		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Type == TargetType.Editor)
		{
			PublicDefinitions.Add("WITH_NNE_RUNTIME_COREML");
		}
		
		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.AddRange(new string[] { "CoreML" });
		}
	}
}
