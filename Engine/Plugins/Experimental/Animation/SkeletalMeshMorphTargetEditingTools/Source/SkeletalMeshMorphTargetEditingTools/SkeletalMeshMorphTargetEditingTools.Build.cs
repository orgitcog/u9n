// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SkeletalMeshMorphTargetEditingTools : ModuleRules
{
	public SkeletalMeshMorphTargetEditingTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
				"ModelingToolsEditorMode",
				"SkeletalMeshModelingTools"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore", 
				"ModelingComponents", 
				"MeshConversion", 
				"MeshModelingTools", 
				"MeshModelingToolsEditorOnly", 
				"InteractiveToolsFramework",
				"SkeletalMeshUtilitiesCommon",
				"GeometryCore",
				"SkeletalMeshModifiers",
				"MeshDescription",
				"StaticMeshDescription",
				"SkeletalMeshDescription",
				"ModelingComponentsEditorOnly",
				"UnrealEd",
				"Persona",
				"GeometryFramework", 
				"SkeletalMeshEditor"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
		
		ShortName = "SKMMorphTools";
	}
}
