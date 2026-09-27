// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationsModule.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "WaveformTransformationMarkersObjectCustomization.h"


void FWaveformTransformationsModule::StartupModule()
{
	IModuleInterface::StartupModule();
	
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		
		PropertyEditorModule.RegisterCustomPropertyTypeLayout(
			"WaveformTransformationMarkers", 
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FWaveformTransformationMarkersObjectCustomization::MakeInstance)
		);
	}
}

void FWaveformTransformationsModule::ShutdownModule()
{
	IModuleInterface::ShutdownModule();

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyEditorModule.UnregisterCustomClassLayout("WaveformTransformationMarkers");
	}
}

IMPLEMENT_MODULE(FWaveformTransformationsModule, WaveformTransformations);