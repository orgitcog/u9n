// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MutableClothing : ModuleRules
{
	public MutableClothing(ReadOnlyTargetRules TargetRules) : base(TargetRules)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ClothingSystemRuntimeCommon",
			});
		
        PublicDependencyModuleNames.AddRange(
	        new string[]
	        {
		        "CustomizableObject",
     		});
    }
}
