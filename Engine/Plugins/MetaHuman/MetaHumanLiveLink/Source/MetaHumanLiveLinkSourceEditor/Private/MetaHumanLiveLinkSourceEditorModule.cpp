// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanLiveLinkSourceEditorModule.h"
#include "MetaHumanLiveLinkSourceStyle.h"

#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

#include "MetaHumanSmoothingPreProcessor.h"
#include "MetaHumanSmoothingPreProcessorCustomization.h"
#include "MetaHumanLiveLinkSubjectSettings.h"
#include "MetaHumanLiveLinkSubjectSettingsCustomization.h"

void FMetaHumanLiveLinkSourceEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	const FName& SmoothingPreProcessorClassName = UMetaHumanSmoothingPreProcessor::StaticClass()->GetFName();
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(SmoothingPreProcessorClassName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaHumanSmoothingPreProcessorCustomization::MakeInstance));

	PropertiesToUnregisterOnShutdown.Add(SmoothingPreProcessorClassName);

	PropertyEditorModule.RegisterCustomClassLayout(UMetaHumanLiveLinkSubjectSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanLiveLinkSubjectSettingsCustomization::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UMetaHumanLiveLinkSubjectSettings::StaticClass()->GetFName());

	FMetaHumanLiveLinkSourceStyle::Register();
}

void FMetaHumanLiveLinkSourceEditorModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	for (const FName& PropertyToUnregisterOnShutdown : PropertiesToUnregisterOnShutdown)
	{
		PropertyEditorModule.UnregisterCustomPropertyTypeLayout(PropertyToUnregisterOnShutdown);
	}

	for (const FName& ClassToUnregisterOnShutdown : ClassesToUnregisterOnShutdown)
	{
		PropertyEditorModule.UnregisterCustomClassLayout(ClassToUnregisterOnShutdown);
	}

	FMetaHumanLiveLinkSourceStyle::Unregister();
}

IMPLEMENT_MODULE(FMetaHumanLiveLinkSourceEditorModule, MetaHumanLiveLinkSourceEditor)
