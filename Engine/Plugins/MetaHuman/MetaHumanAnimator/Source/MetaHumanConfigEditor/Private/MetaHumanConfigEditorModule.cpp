// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "MetaHumanConfig.h"
#include "Customizations/MetaHumanConfigCustomizations.h"
#include "PropertyEditorModule.h"

class FMetaHumanConfigEditorModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** StaticClass is not safe on shutdown, so we cache the name, and use this to unregister on shut down */
	FName ClassToUnregisterOnShutdown;

};

void FMetaHumanConfigEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	ClassToUnregisterOnShutdown = UMetaHumanConfig::StaticClass()->GetFName();
	PropertyEditorModule.RegisterCustomClassLayout(ClassToUnregisterOnShutdown, FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanConfigCustomization::MakeInstance));
}

void FMetaHumanConfigEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyEditorModule.UnregisterCustomClassLayout(ClassToUnregisterOnShutdown);
	}
}

IMPLEMENT_MODULE(FMetaHumanConfigEditorModule, MetaHumanConfigEditor)