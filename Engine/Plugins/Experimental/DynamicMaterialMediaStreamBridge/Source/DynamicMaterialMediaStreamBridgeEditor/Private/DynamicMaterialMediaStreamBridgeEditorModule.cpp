// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicMaterialMediaStreamBridgeEditorModule.h"

#include "DMMaterialValueMediaStream.h"
#include "DMMaterialValueMediaStreamPropertyRowGenerator.h"
#include "DMMediaStreamStageSourceMenuExtender.h"
#include "IDynamicMaterialEditorModule.h"
#include "Modules/ModuleManager.h"

void FDynamicMaterialMediaStreamBridgeEditorModule::StartupModule()
{
	IDynamicMaterialEditorModule& DMEditorModule = IDynamicMaterialEditorModule::Get();
	DMEditorModule.RegisterComponentPropertyRowGeneratorDelegate<UDMMaterialValueMediaStream, FDMMaterialValueMediaStreamPropertyRowGenerator>();

	FDMMediaStreamStageSourceMenuExtender::Get().Integrate();
}

IMPLEMENT_MODULE(FDynamicMaterialMediaStreamBridgeEditorModule, DynamicMaterialMediaStreamBridgeEditor)
