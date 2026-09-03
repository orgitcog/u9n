// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDefaultEditorPipelineLog.h"
#include "MetaHumanDefaultEditorPipelineBase.h"

#include "Customizations/MetaHumanMaterialBakingOptionsDetailCustomization.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

class FMetaHumanDefaultEditorPipelineModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

		PropertyModule.RegisterCustomPropertyTypeLayout(FMetaHumanMaterialBakingOptions::StaticStruct()->GetFName(),
														FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaHumanMaterialBakingOptionsDetailCustomziation::MakeInstance));
	}

	virtual void ShutdownModule() override
	{
		if (FPropertyEditorModule* PropertyModulePtr = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		{
			PropertyModulePtr->UnregisterCustomPropertyTypeLayout(FMetaHumanMaterialBakingOptions::StaticStruct()->GetFName());
		}
	}
};

IMPLEMENT_MODULE(FMetaHumanDefaultEditorPipelineModule, MetaHumanDefaultEditorPipeline);

DEFINE_LOG_CATEGORY(LogMetaHumanDefaultEditorPipeline);
