// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "IpAddressDetailsCustomization.h"
#include "MetaHumanCaptureSource.h"
#include "MetaHumanCaptureSourceSync.h"
#include "MetaHumanCaptureSourceAutoReimport.h"

#include "Settings/EditorLoadingSavingSettings.h"

#define MHCAPTURESOURCE_CHECK_AND_RETURN(Func) if (bool Condition = Func; !Condition) { return Condition; }

namespace UE::MetaHuman
{
static bool CompareProperties(const UClass* const InLeftClass, const UClass* const InRightClass)
{
	// We need to exclude the UMetaHumanCaptureSourceSync::MetaHumanCaptureSource property, as this exists only in the synchronous form of the capture 
	// source and is needed to manage garbage collection.
	const TArray<FName> ExcludedPropertyNames = {
		TEXT("MetaHumanCaptureSource"),
	};

	for (TFieldIterator<FProperty> PropertyIter(InLeftClass, EFieldIterationFlags::None); PropertyIter; ++PropertyIter)
	{
		const FName PropertyName = PropertyIter->GetFName();
		const bool bIsExcludedProperty = ExcludedPropertyNames.Contains(PropertyName);

		if (!bIsExcludedProperty && InRightClass->FindPropertyByName(PropertyName) == nullptr)
		{
			return false;
		}
	}

	return true;
}

} // namespace UE::MetaHuman

class FMetaHumanCaptureSourceModule
	: public IModuleInterface
{
public:

	//~Begin IModuleInterface interface
	virtual void StartupModule() override
	{
		checkf(VerifyCaptureSourcesProperties(), TEXT("There is a mismatch between MetaHumanCaptureSource and MetaHumanCaptureSourceSync properties"));
		PostEngineInitDelegateHandle = FCoreDelegates::OnPostEngineInit.Add(FSimpleDelegate::CreateStatic(&FMetaHumanCaptureSourceModule::PostEngineInit));

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout("DeviceAddress", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FIpAddressDetailsCustomization::MakeInstance));

		PropertyModule.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			// Unregister properties when the module is shutdown
			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomPropertyTypeLayout("DeviceAddress");

			PropertyModule.NotifyCustomizationModuleChanged();
		}

		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitDelegateHandle);
	}
	//~End IModuleInterface interface

private:
	static void PostEngineInit()
	{
		using namespace UE::MetaHuman;

		UEditorLoadingSavingSettings* Settings = GetMutableDefault<UEditorLoadingSavingSettings>();

		if (!ensure(IsValid(Settings)))
		{
			return;
		}

		// We are doing the same operation in the depth generator module, so make sure they're both using the game thread
		check(IsInGameThread());

		// Note: We use /Game for both UE and UEFN. We rely on the project root being mapped to /Game in UEFN.
		const TArray<FAutoReimportDirectoryConfig> DirectoryConfigs = CaptureSourceUpdateAutoReimportExclusion(
			TEXT("/Game/"),
			TEXT("*_Ingested/*"), 
			Settings->AutoReimportDirectorySettings
		);

		if (CaptureSourceDirectoryConfigsAreDifferent(Settings->AutoReimportDirectorySettings, DirectoryConfigs))
		{
			Settings->AutoReimportDirectorySettings = DirectoryConfigs;
			Settings->SaveConfig();
			Settings->OnSettingChanged().Broadcast(GET_MEMBER_NAME_CHECKED(UEditorLoadingSavingSettings, AutoReimportDirectorySettings));
		}
	}

	static bool VerifyCaptureSourcesProperties()
	{
		using namespace UE::MetaHuman;

		MHCAPTURESOURCE_CHECK_AND_RETURN(CompareProperties(UMetaHumanCaptureSourceSync::StaticClass(), UMetaHumanCaptureSource::StaticClass()));
		MHCAPTURESOURCE_CHECK_AND_RETURN(CompareProperties(UMetaHumanCaptureSource::StaticClass(), UMetaHumanCaptureSourceSync::StaticClass()));

		return true;
	}

	FDelegateHandle PostEngineInitDelegateHandle;
};

IMPLEMENT_MODULE(FMetaHumanCaptureSourceModule, MetaHumanCaptureSource)

#undef MHCAPTURESOURCE_CHECK_AND_RETURN
