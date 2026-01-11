// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDUtilitiesModule.h"

#include "USDErrorUtils.h"
#include "USDMemory.h"

#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "MessageLogModule.h"
#endif	  // WITH_EDITOR

class FUsdUtilitiesModule : public IUsdUtilitiesModule
{
	FDelegateHandle OnProjectSettingsChangedHandle;

public:
	virtual void StartupModule() override
	{
		UUsdProjectSettings* ProjectSettings = GetMutableDefault<UUsdProjectSettings>();
		if (ProjectSettings->bLogUsdSdkErrors)
		{
			FUsdLogManager::RegisterDiagnosticDelegate();
		}

#if WITH_EDITOR
		LLM_SCOPE_BYTAG(Usd);

		OnProjectSettingsChangedHandle = ProjectSettings->OnSettingChanged().AddLambda(
			[](UObject* SettingsObject, struct FPropertyChangedEvent& PropertyChangedEvent)
			{
				UUsdProjectSettings* UsdSettings = Cast<UUsdProjectSettings>(SettingsObject);
				if (!UsdSettings)
				{
					return;
				}

				if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UUsdProjectSettings, bLogUsdSdkErrors))
				{
					if (UsdSettings->bLogUsdSdkErrors)
					{
						FUsdLogManager::RegisterDiagnosticDelegate();
					}
					else
					{
						FUsdLogManager::UnregisterDiagnosticDelegate();
					}
				}
			}
		);

		FMessageLogInitializationOptions InitOptions;
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
		MessageLogModule.RegisterLogListing(TEXT("USD"), NSLOCTEXT("USDUtilitiesModule", "USDLogListing", "USD"), InitOptions);
#endif	  // WITH_EDITOR
	}

	virtual void ShutdownModule() override
	{
		FUsdLogManager::UnregisterDiagnosticDelegate();

#if WITH_EDITOR
		// We can't query default objects during engine exit
		if (UObjectInitialized())
		{
			UUsdProjectSettings* ProjectSettings = GetMutableDefault<UUsdProjectSettings>();
			if (ProjectSettings)
			{
				ProjectSettings->OnSettingChanged().Remove(OnProjectSettingsChangedHandle);
			}
		}

		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
		MessageLogModule.UnregisterLogListing(TEXT("USD"));
#endif	  // WITH_EDITOR
	}
};

IMPLEMENT_MODULE_USD(FUsdUtilitiesModule, USDUtilities);
