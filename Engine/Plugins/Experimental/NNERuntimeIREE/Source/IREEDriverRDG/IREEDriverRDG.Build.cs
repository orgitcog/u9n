// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IREEDriverRDG : ModuleRules
{
	public IREEDriverRDG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange
		(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"IREE",
				"NNE",
				"Projects",
				"RenderCore",
				"RHI",
				"NNERuntimeIREEShader"
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Json",
					"ShaderCompilerCommon"
				}
			);
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("WITH_IREE_DRIVER_RDG");
			PrivateDefinitions.Add("IREE_DRIVER_RDG_VERBOSITY=0");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicDefinitions.Add("WITH_IREE_DRIVER_RDG");
			PrivateDefinitions.Add("IREE_DRIVER_RDG_VERBOSITY=0");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDefinitions.Add("WITH_IREE_DRIVER_RDG");
			PrivateDefinitions.Add("IREE_DRIVER_RDG_VERBOSITY=0");
		}
	}
}
