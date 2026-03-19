// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RigVMEditor : ModuleRules
{
    public RigVMEditor(ReadOnlyTargetRules Target) : base(Target)
    {
	    CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "RigVM",
                "RigVMDeveloper",
                "SlateCore",
                "BlueprintGraph",
                "GraphEditor",
                "UnrealEd",
                "Slate",
                "SlateCore",
                "UMG",
                "InputCore",
                "ToolWidgets",
                "KismetCompiler",
                "KismetWidgets",
                "AnimationWidgets",
                "ApplicationCore",
                "AppFramework",
                "AnimationCore",
                "PropertyEditor",
                "ToolMenus",
                "MessageLog",
                "StructUtilsEditor",
                "ContentBrowser",
                "ContentBrowserData",
                "EditorFramework",
                "AnimationEditorWidgets",
                "WidgetRegistration",
            }
		);
        
        bool bAddBlueprintEditorDependency = true;
        PublicDefinitions.Add("WITH_RIGVMLEGACYEDITOR=" + (bAddBlueprintEditorDependency ? '1' : '0'));
        if (bAddBlueprintEditorDependency)
        {
			PublicDependencyModuleNames.Add("Kismet");
		}
        else
        {
	        PrivateDependencyModuleNames.AddRange(new string[]
	        {
		        "ApplicationCore",
		        "FieldNotification",
		        "InputCore",
		        "Json",
		        "JsonUtilities",
		        "ToolWidgets",
		        "TraceLog",
		        "WorkspaceMenuStructure"
	        });
        }
	}
}
