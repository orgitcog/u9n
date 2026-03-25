// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "MetaHumanFaceAnimationSolver.h"
#include "Customizations/MetaHumanFaceAnimationSolverCustomizations.h"
#include "PropertyEditorModule.h"

class FMetaHumanFaceAnimationSolverEditorModule
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

void FMetaHumanFaceAnimationSolverEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	ClassToUnregisterOnShutdown = UMetaHumanFaceAnimationSolver::StaticClass()->GetFName();
	PropertyEditorModule.RegisterCustomClassLayout(ClassToUnregisterOnShutdown, FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanFaceAnimationSolverCustomization::MakeInstance));
}

void FMetaHumanFaceAnimationSolverEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyEditorModule.UnregisterCustomClassLayout(ClassToUnregisterOnShutdown);
	}
}

IMPLEMENT_MODULE(FMetaHumanFaceAnimationSolverEditorModule, MetaHumanFaceAnimationSolverEditor)