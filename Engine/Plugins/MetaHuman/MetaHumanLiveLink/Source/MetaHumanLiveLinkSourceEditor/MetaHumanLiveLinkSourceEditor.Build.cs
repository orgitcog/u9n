// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class MetaHumanLiveLinkSourceEditor : ModuleRules
{
	public MetaHumanLiveLinkSourceEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"LiveLinkInterface",
			"UnrealEd",
		});
		
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"MetaHumanLiveLinkSource",
			"MetaHumanCoreTech"
		});
	}
}
