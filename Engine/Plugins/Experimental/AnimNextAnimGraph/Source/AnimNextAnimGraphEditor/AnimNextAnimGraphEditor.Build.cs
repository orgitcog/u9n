// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextAnimGraphEditor : ModuleRules
	{
		public AnimNextAnimGraphEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"UnrealEd",
					"InputCore",
					"SlateCore",
					"Slate",
					"AnimNext", 
					"AnimNextEditor", 
					"AnimNextUncookedOnly",
					"AnimNextAnimGraph",
					"AnimNextAnimGraphUncookedOnly",
					"Settings",
					"WorkspaceEditor", 
					"EditorWidgets", 
					"MessageLog", 
					"AssetDefinition",
					"RigVM",
					"RigVMDeveloper",
					"ToolMenus", 
					"RigVMEditor",
					"GraphEditor",
					"Persona",
					"PropertyEditor",
					"TraceAnalysis",
					"TraceLog",
					"TraceServices",
					"TraceInsights",
					"RewindDebuggerInterface",
					"GameplayInsights",
				}
			);
		}
	}
}