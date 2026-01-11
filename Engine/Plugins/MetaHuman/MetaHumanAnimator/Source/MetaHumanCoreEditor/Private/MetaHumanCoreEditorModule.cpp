// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCoreEditorModule.h"

#include "MetaHumanEditorSettings.h"

#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "MetaHumanCoreEditor"

class FMetaHumanCoreEditorModule
	: public IMetaHumanCoreEditorModule
{
public:

	//~Begin IModuleInterface interface
	virtual void StartupModule() override
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings(TEXT("Editor"), TEXT("Plugins"), TEXT("MetaHuman_Settings"),
											 LOCTEXT("SettingsName", "MetaHuman"), LOCTEXT("SettingsDescription", "Configure MetaHuman settings"),
											 GetMutableDefault<UMetaHumanEditorSettings>());
		}
	}

	virtual void ShutdownModule() override
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings(TEXT("Editor"), TEXT("Plugins"), TEXT("MetaHuman_Settings"));
		}
	}
	//~Begin IModuleInterface interface

	//~Begin IMetaHumanCoreEditorModule interface
	virtual TConstArrayView<FAssetCategoryPath> GetMetaHumanAssetCategoryPath() const override
	{
		static FAssetCategoryPath Categories[] = { MetaHumanAssetCategoryPath };
		return Categories;
	}

	virtual TConstArrayView<FAssetCategoryPath> GetMetaHumanAdvancedAssetCategoryPath() const override
	{
		static FAssetCategoryPath Categories[] = { MetaHumanAdvancedAssetCategoryPath };
		return Categories;
	}
	//~End IMetaHumanCoreEditorModule interface

private:
	const FAssetCategoryPath MetaHumanAssetCategoryPath{ LOCTEXT("MetaHumanAssetCategoryLabel", "MetaHuman") };
	const FAssetCategoryPath MetaHumanAdvancedAssetCategoryPath{ MetaHumanAssetCategoryPath.GetCategoryText(), LOCTEXT("MetaHumanAdvancedAssetCategoryLabel", "Advanced") };
};

IMPLEMENT_MODULE(FMetaHumanCoreEditorModule, MetaHumanCoreEditor)

#undef LOCTEXT_NAMESPACE