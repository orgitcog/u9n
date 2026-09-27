// Copyright Epic Games, Inc. All Rights Reserved.

#include "SKMMorphTargetEditingToolsModule.h"

#include "MorphTargetVertexSculptTool.h"
#include "SKMMorphTargetEditingToolsCommands.h"
#include "SKMMorphTargetEditingToolsStyle.h"
#include "SKMMorphTargetToolTarget.h"
#include "Features/IModularFeatures.h"
#include "Framework/Commands/Commands.h"

#define LOCTEXT_NAMESPACE "FSkeletalMeshMorphTargetEditingToolsModule"

void FSkeletalMeshMorphTargetEditingToolsModule::StartupModule()
{
	FSkeletalMeshMorphTargetEditingToolsStyle::Register();
	FSkeletalMeshMorphTargetEditingToolsCommands::Register();
	IModularFeatures::Get().RegisterModularFeature(ISkeletalMeshModelingModeToolExtension::GetModularFeatureName(), this);	

}

void FSkeletalMeshMorphTargetEditingToolsModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(ISkeletalMeshModelingModeToolExtension::GetModularFeatureName(), this);
	FSkeletalMeshMorphTargetEditingToolsCommands::Unregister();
	FSkeletalMeshMorphTargetEditingToolsStyle::Unregister();
}



void FSkeletalMeshMorphTargetEditingToolsModule::GetExtensionTools(const FExtensionToolQueryInfo& QueryInfo,
                                                                   TArray<FExtensionToolDescription>& OutTools)
{
	const FSkeletalMeshMorphTargetEditingToolsCommands& Commands = FSkeletalMeshMorphTargetEditingToolsCommands::Get();
	{
		FExtensionToolDescription ToolDesc;
		ToolDesc.ToolName = LOCTEXT("SkeletalMeshMorphTargetSculptTool", "Sculpt Morph Target");
		ToolDesc.ToolCommand = Commands.BeginMorphTargetSculptTool;
		ToolDesc.ToolBuilder = NewObject<UMorphTargetVertexSculptToolBuilder>();
		OutTools.Add(ToolDesc);
	}
}

bool FSkeletalMeshMorphTargetEditingToolsModule::GetExtensionExtendedInfo(FModelingModeExtensionExtendedInfo& InfoOut)
{
	InfoOut.ExtensionCommand = FSkeletalMeshMorphTargetEditingToolsCommands::Get().BeginMorphTargetTool;
	
	return true;
}

bool FSkeletalMeshMorphTargetEditingToolsModule::GetExtensionToolTargets(TArray<TSubclassOf<UToolTargetFactory>>& ToolTargetFactoriesOut)
{
	ToolTargetFactoriesOut.Add(USkeletalMeshMorphTargetToolTargetFactory::StaticClass());
	
	return true;
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSkeletalMeshMorphTargetEditingToolsModule, SkeletalMeshMorphTargetEditingTools)