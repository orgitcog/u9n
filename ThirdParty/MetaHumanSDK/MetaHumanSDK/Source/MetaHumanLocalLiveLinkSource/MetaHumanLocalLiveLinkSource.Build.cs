// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class MetaHumanLocalLiveLinkSource : ModuleRules
{
	public MetaHumanLocalLiveLinkSource(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"MetaHumanLiveLinkSource",
		});
			
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"LiveLink",
			"LiveLinkInterface",
			"MediaUtils",
			"MediaAssets",
			"MediaFrameworkUtilities",
			"SlateCore",
			"Slate",
			"InputCore",
			"Engine",
			"AudioPlatformConfiguration",
			"MetaHumanPipelineCore",
			"MetaHumanImageViewer",
			"MetaHumanCoreTech"
		});

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
