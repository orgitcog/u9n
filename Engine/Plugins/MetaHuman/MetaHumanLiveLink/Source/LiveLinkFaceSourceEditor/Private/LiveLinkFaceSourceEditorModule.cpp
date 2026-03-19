// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceSourceEditorModule.h"

#include "ISettingsModule.h"
#include "LiveLinkFaceSourceCustomization.h"
#include "LiveLinkFaceSourceSettings.h"
#include "LiveLinkFaceSourceDefaults.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "LiveLinkFaceSourceEditor"

void FLiveLinkFaceSourceEditorModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FLiveLinkFaceSourceEditorModule::PostEngineInit);
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FLiveLinkFaceSourceEditorModule::EnginePreExit);

	const FName& SettingsName = ULiveLinkFaceSourceSettings::StaticClass()->GetFName();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout(SettingsName, FOnGetDetailCustomizationInstance::CreateStatic(&FLiveLinkFaceSourceCustomization::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(SettingsName);
};

void FLiveLinkFaceSourceEditorModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	for (const FName& ClassToUnregisterOnShutdown : ClassesToUnregisterOnShutdown)
	{
		PropertyEditorModule.UnregisterCustomClassLayout(ClassToUnregisterOnShutdown);
	}
}

void FLiveLinkFaceSourceEditorModule::PostEngineInit()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		ULiveLinkFaceSourceDefaults* Settings = GetMutableDefault<ULiveLinkFaceSourceDefaults>();
		check(Settings);

		SettingsModule->RegisterSettings("Project", "Plugins", "Live Link Face",
			LOCTEXT("LiveLinkFaceSourceSettingsDisplayName", "Live Link Face"),
			LOCTEXT("LiveLinkFaceSourceSettingsDescription", "Settings for the Live Link Face source."),
			Settings
		);
	}
}

void FLiveLinkFaceSourceEditorModule::EnginePreExit()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Live Link Face");
	}
}

IMPLEMENT_MODULE(FLiveLinkFaceSourceEditorModule, LiveLinkFaceSourceEditor)

#undef LOCTEXT_NAMESPACE
