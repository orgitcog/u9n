// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DynamicMaterialMediaStreamBridge : ModuleRules
{
	public DynamicMaterialMediaStreamBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DynamicMaterial"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"MediaStream"
			}
		);

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"SlateCore"
			});
		}

		ShortName = "DMMSBridge";
	}
}
