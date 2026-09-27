// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataflowEditor : ModuleRules
	{
		public DataflowEditor(ReadOnlyTargetRules Target) : base(Target)
		{

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"TedsOutliner",
				}
			);

			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"SkeletonEditor"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AdvancedPreviewScene",
					"ApplicationCore",
					"AssetDefinition",
					"AssetTools",
					"AssetRegistry",
					"BaseCharacterFXEditor",
					"BlueprintGraph",
					"Chaos",
					"Core",
					"CoreUObject",
					"DataflowAssetTools",
					"DataflowCore",
					"DataflowEngine",
					"DataflowEnginePlugin",
					"DataflowNodes",
					"DataflowSimulation",
					"DeveloperSettings",
					"DynamicMesh",
					"Engine",
					"EditorFramework",
					"EditorInteractiveToolsFramework",
					"EditorStyle",
					"GeometryCache",
					"GeometryCore",
					"GeometryCollectionEngine",
					"GeometryFramework",
					"GraphEditor",
					"InputCore",
					"InteractiveToolsFramework",
					"LevelEditor",
					"MeshDescription",
					"MeshConversion",
					"MeshModelingTools",
					"MeshModelingToolsEditorOnly",
					"MeshModelingToolsEditorOnlyExp",
					"MeshModelingToolsExp",
					"ModelingComponentsEditorOnly",
					"ModelingComponents",
					"Projects",
					"PropertyEditor",
					"RenderCore",
					"RHI",
					"SceneOutliner",
					"TedsOutliner",
					"SharedSettingsWidgets",
					"SkeletonEditor",
					"Slate",
					"SlateCore",
					"StaticMeshDescription",
					"ToolMenus",
					"ToolWidgets",
					"TypedElementRuntime",
					"TypedElementFramework",
					"UnrealEd",
					"WorkspaceMenuStructure",
					"XmlParser",
					"EditorWidgets",
					"KismetWidgets",      // SScrubControlPanel
					"AnimGraph", 
					"ChaosCaching", // UAnimSingleNodeInstance
					"StructUtilsEditor",
					"MessageLog",
					"AppFramework",
					"SkeletalMeshDescription"
				}
			);
		}
	}
}
