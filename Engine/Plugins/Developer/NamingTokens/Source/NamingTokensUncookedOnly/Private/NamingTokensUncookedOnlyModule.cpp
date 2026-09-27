// Copyright Epic Games, Inc. All Rights Reserved.

#include "NamingTokensUncookedOnlyModule.h"

#include "Customization/NamingTokensCustomization.h"
#include "NamingTokenData.h"
#include "NamingTokens.h"
#include "NamingTokensStyle.h"

#include "PropertyEditorModule.h"

void FNamingTokensUncookedOnlyModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomClassLayout(
		UNamingTokens::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FNamingTokensCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout(FNamingTokenData::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNamingTokensDataCustomization::MakeInstance));

	FNamingTokensStyle::Get();
}

void FNamingTokensUncookedOnlyModule::ShutdownModule()
{
	if (UObjectInitialized() && !IsEngineExitRequested())
	{
		if (FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyEditorModule->UnregisterCustomClassLayout(UNamingTokens::StaticClass()->GetFName());
			PropertyEditorModule->UnregisterCustomPropertyTypeLayout(FNamingTokenData::StaticStruct()->GetFName());
		}
	}
}

IMPLEMENT_MODULE(FNamingTokensUncookedOnlyModule, NamingTokensUncookedOnly)
