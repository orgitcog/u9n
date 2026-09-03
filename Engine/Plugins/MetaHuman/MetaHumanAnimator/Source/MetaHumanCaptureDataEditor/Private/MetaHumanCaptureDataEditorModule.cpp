// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCaptureDataEditorModule.h"
#include "CaptureData.h"
#include "Customizations/CaptureDataCustomizations.h"
#include "PropertyEditorModule.h"



void FMetaHumanCaptureDataEditorModule::StartupModule()
{
	ClassToUnregisterOnShutdown = UFootageCaptureData::StaticClass()->GetFName();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout(ClassToUnregisterOnShutdown, FOnGetDetailCustomizationInstance::CreateStatic(&FFootageCaptureDataCustomization::MakeInstance));
}

void FMetaHumanCaptureDataEditorModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.UnregisterCustomClassLayout(ClassToUnregisterOnShutdown);
}

IMPLEMENT_MODULE(FMetaHumanCaptureDataEditorModule, MetaHumanCaptureDataEditor)