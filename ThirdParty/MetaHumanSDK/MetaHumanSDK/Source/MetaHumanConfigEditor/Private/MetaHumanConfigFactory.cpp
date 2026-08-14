// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanConfigFactory.h"
#include "MetaHumanConfig.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanConfigFactory)

//////////////////////////////////////////////////////////////////////////
// UMetaHumanConfigFactory

UMetaHumanConfigFactory::UMetaHumanConfigFactory()
{
	Formats.Add(TEXT("json;Json data file"));
	Formats.Add(TEXT("bin;Binary file"));

	SupportedClass = UMetaHumanConfig::StaticClass();
	bEditorImport = true;
}

bool UMetaHumanConfigFactory::FactoryCanImport(const FString& InFilename)
{
	const IConsoleVariable* AllowCustomizationVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("mh.Config.AllowCustomization"));
	check(AllowCustomizationVariable);
	if (!AllowCustomizationVariable->GetBool())
	{
		return false;
	}

	UMetaHumanConfig *Config = NewObject<UMetaHumanConfig>(GetTransientPackage(), UMetaHumanConfig::StaticClass());

	return Config->ReadFromDirectory(FPaths::GetPath(InFilename));
}

UObject* UMetaHumanConfigFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, const FString& InFilename, const TCHAR* InParms, FFeedbackContext* InWarn, bool& bOutOperationCanceled)
{
	UMetaHumanConfig* Config = NewObject<UMetaHumanConfig>(InParent, InClass, InName, InFlags);

	Config->ReadFromDirectory(FPaths::GetPath(InFilename));

	return Config;
}
