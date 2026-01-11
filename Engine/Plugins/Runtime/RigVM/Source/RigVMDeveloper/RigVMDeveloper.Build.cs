// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RigVMDeveloper : ModuleRules
{
    public RigVMDeveloper(ReadOnlyTargetRules Target) : base(Target)
    {
	    CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "RigVM",
                "VisualGraphUtils",
                "KismetCompiler",
            }
        );

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
					"EditorFramework",
                    "UnrealEd",
					"Slate",
					"SlateCore",
					"MessageLog",
					"BlueprintGraph",
					"GraphEditor",
                }
			);

            bool bAddBlueprintEditorDependency = true;
            PublicDefinitions.Add("WITH_RIGVMLEGACYEDITOR=" + (bAddBlueprintEditorDependency ? '1' : '0'));
            if (bAddBlueprintEditorDependency)
            {
	            PrivateDependencyModuleNames.Add("Kismet");
            }
            
            PrivateIncludePathModuleNames.Add("RigVMEditor");
            DynamicallyLoadedModuleNames.Add("RigVMEditor");
        }
    }
}
