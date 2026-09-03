// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class MetaHumanLocalLiveLinkSourceEditor : ModuleRules
{
	public MetaHumanLocalLiveLinkSourceEditor(ReadOnlyTargetRules Target) : base(Target)
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
			"EditorStyle",
			"MetaHumanCoreTech",
			"MetaHumanImageViewer",
			"MetaHumanPipelineCore",
			"MetaHumanLocalLiveLinkSource",
			"MetaHumanLiveLinkSourceEditor",
		});
	}
}
