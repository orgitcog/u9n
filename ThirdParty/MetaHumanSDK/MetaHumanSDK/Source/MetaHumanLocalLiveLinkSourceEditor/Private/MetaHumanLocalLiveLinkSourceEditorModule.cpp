// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanVideoLiveLinkSourceSettings.h"
#include "MetaHumanVideoLiveLinkSourceCustomization.h"
#include "MetaHumanVideoBaseLiveLinkSubjectSettings.h"
#include "MetaHumanVideoBaseLiveLinkSubjectCustomization.h"
#include "MetaHumanAudioLiveLinkSourceSettings.h"
#include "MetaHumanAudioLiveLinkSourceCustomization.h"
#include "MetaHumanAudioBaseLiveLinkSubjectSettings.h"
#include "MetaHumanAudioBaseLiveLinkSubjectCustomization.h"
#include "MetaHumanVideoLiveLinkSettings.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "MetaHumanLocalLiveLinkSourceEditor"



class FMetaHumanLocalLiveLinkSourceEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** StaticClass is not safe on shutdown, so we cache the name, and use this to unregister on shut down */
	TArray<FName> ClassesToUnregisterOnShutdown;

	void PostEngineInit();
	void EnginePreExit();
};

void FMetaHumanLocalLiveLinkSourceEditorModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FMetaHumanLocalLiveLinkSourceEditorModule::PostEngineInit);
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FMetaHumanLocalLiveLinkSourceEditorModule::EnginePreExit);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// RegisterCustomClassLayout of base class last in order to ensure derived classes are applied first

	PropertyEditorModule.RegisterCustomClassLayout(UMetaHumanVideoLiveLinkSourceSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanVideoLiveLinkSourceCustomization::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UMetaHumanVideoLiveLinkSourceSettings::StaticClass()->GetFName());

	PropertyEditorModule.RegisterCustomClassLayout(UMetaHumanVideoBaseLiveLinkSubjectSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanVideoBaseLiveLinkSubjectCustomization::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UMetaHumanVideoBaseLiveLinkSubjectSettings::StaticClass()->GetFName());

	PropertyEditorModule.RegisterCustomClassLayout(UMetaHumanAudioLiveLinkSourceSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanAudioLiveLinkSourceCustomization::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UMetaHumanAudioLiveLinkSourceSettings::StaticClass()->GetFName());

	PropertyEditorModule.RegisterCustomClassLayout(UMetaHumanAudioBaseLiveLinkSubjectSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanAudioBaseLiveLinkSubjectCustomization::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UMetaHumanAudioBaseLiveLinkSubjectSettings::StaticClass()->GetFName());

	PropertyEditorModule.RegisterCustomClassLayout(UMetaHumanLocalLiveLinkSubjectSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanLocalLiveLinkSubjectCustomization::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UMetaHumanLocalLiveLinkSubjectSettings::StaticClass()->GetFName());
};

void FMetaHumanLocalLiveLinkSourceEditorModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	for (const FName& ClassToUnregisterOnShutdown : ClassesToUnregisterOnShutdown)
	{
		PropertyEditorModule.UnregisterCustomClassLayout(ClassToUnregisterOnShutdown);
	}
}

void FMetaHumanLocalLiveLinkSourceEditorModule::PostEngineInit()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		UMetaHumanVideoLiveLinkSettings* Settings = GetMutableDefault<UMetaHumanVideoLiveLinkSettings>();
		check(Settings);

		SettingsModule->RegisterSettings("Project", "Plugins", "MetaHuman Live Link Video",
			LOCTEXT("MetaHumanSettingsName", "MetaHuman Live Link (Video)"),
			LOCTEXT("MetaHumanDescription", "Configure MetaHuman Video Live Link."),
			Settings
		);
	}
}

void FMetaHumanLocalLiveLinkSourceEditorModule::EnginePreExit()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "MetaHuman Live Link Video");
	}
}

IMPLEMENT_MODULE(FMetaHumanLocalLiveLinkSourceEditorModule, MetaHumanLocalLiveLinkSourceEditor)

#undef LOCTEXT_NAMESPACE
