// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class MetaHumanLiveLinkSource : ModuleRules
{
	public MetaHumanLiveLinkSource(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"LiveLinkInterface",
			"LiveLink",
			"Engine",
		});
			
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"RenderCore",
			"SlateCore",
			"Slate",
			"MetaHumanCoreTech"
		});
	}
}
