// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "MetaHumanFaceFittingSolver.h"
#include "Customizations/MetaHumanFaceFittingSolverCustomizations.h"
#include "PropertyEditorModule.h"

class FMetaHumanFaceFittingSolverEditorModule
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

void FMetaHumanFaceFittingSolverEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	ClassToUnregisterOnShutdown = UMetaHumanFaceFittingSolver::StaticClass()->GetFName();
	PropertyEditorModule.RegisterCustomClassLayout(ClassToUnregisterOnShutdown, FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanFaceFittingSolverCustomization::MakeInstance));
}

void FMetaHumanFaceFittingSolverEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyEditorModule.UnregisterCustomClassLayout(ClassToUnregisterOnShutdown);
	}
}

IMPLEMENT_MODULE(FMetaHumanFaceFittingSolverEditorModule, MetaHumanFaceFittingSolverEditor)
