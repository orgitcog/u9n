// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidDeviceProfileSelectorModule.h"
#include "AndroidDeviceProfileSelector.h"
#include "Modules/ModuleManager.h"
#if WITH_EDITOR

#if WITH_ANDROID_DEVICE_DETECTION
#include "Interfaces/IAndroidDeviceDetection.h"
#include "Interfaces/IAndroidDeviceDetectionModule.h"
#endif
#include "PIEPreviewDeviceSpecification.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"

#endif
#include "Misc/ConfigCacheIni.h"

IMPLEMENT_MODULE(FAndroidDeviceProfileSelectorModule, AndroidDeviceProfileSelector);

DEFINE_LOG_CATEGORY_STATIC(LogAndroidDPSelector, Log, All)

void FAndroidDeviceProfileSelectorModule::StartupModule()
{
}

void FAndroidDeviceProfileSelectorModule::ShutdownModule()
{
}

const FString FAndroidDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	// We are not expecting this module to have GetRuntimeDeviceProfileName called directly.
	// Android ProfileSelectorModule runtime is now in FAndroidDeviceProfileSelectorRuntimeModule.
	// Use GetDeviceProfileName.
	checkNoEntry();
	return FString();
}

#if WITH_EDITOR
void FAndroidDeviceProfileSelectorModule::ExportDeviceParametersToJson(FString& FolderLocation)
{
#if WITH_ANDROID_DEVICE_DETECTION
	IAndroidDeviceDetection* DeviceDetection;
	DeviceDetection = FModuleManager::LoadModuleChecked<IAndroidDeviceDetectionModule>("AndroidDeviceDetection").GetAndroidDeviceDetection();
	DeviceDetection->Initialize(TEXT("ANDROID_HOME"),
#if PLATFORM_WINDOWS
		TEXT("platform-tools\\adb.exe"),
#else
		TEXT("platform-tools/adb"),
#endif
		TEXT("shell getprop"), true);

	TSet<FString> AlreadyExported;

	{
		FScopeLock ExportLock(DeviceDetection->GetDeviceMapLock());

		const TMap<FString, FAndroidDeviceInfo>& Devices = DeviceDetection->GetDeviceMap();
		for (auto DeviceTuple : Devices)
		{
			const FAndroidDeviceInfo& DeviceInfo = DeviceTuple.Value;
			FString DeviceName = FString::Printf(TEXT("%s_%s(OS%s)"), *DeviceInfo.DeviceBrand, *DeviceInfo.Model, *DeviceInfo.HumanAndroidVersion);
			if (!AlreadyExported.Find(DeviceName))
			{
				FString ExportPath = FolderLocation / (DeviceName + TEXT(".json"));
				DeviceDetection->ExportDeviceProfile(ExportPath, DeviceTuple.Key);
				AlreadyExported.Add(DeviceName);
			}
		}
	}
	FPlatformProcess::Sleep(1.0f);
#endif
}

bool FAndroidDeviceProfileSelectorModule::CanExportDeviceParametersToJson()
{
#if WITH_ANDROID_DEVICE_DETECTION
	return true;
#else
	return false;
#endif
}

void FAndroidDeviceProfileSelectorModule::GetDeviceParametersFromJson(FString& JsonLocation, TMap<FName, FString>& OutDeviceParameters)
{
	TSharedPtr<FJsonObject> JsonRootObject;
	FString Json;

	if (FFileHelper::LoadFileToString(Json, *JsonLocation))
	{
		TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(JsonReader, JsonRootObject);
	}

	FPIEPreviewDeviceSpecifications DeviceSpecs;
	if (JsonRootObject.IsValid() && FJsonObjectConverter::JsonAttributesToUStruct(JsonRootObject->Values, FPIEPreviewDeviceSpecifications::StaticStruct(), &DeviceSpecs, 0, 0))
	{
		FPIEAndroidDeviceProperties& AndroidProperties = DeviceSpecs.AndroidProperties;
		OutDeviceParameters.Add(FName(TEXT("SRC_GPUFamily")), AndroidProperties.GPUFamily);
		OutDeviceParameters.Add(FName(TEXT("SRC_GLVersion")), AndroidProperties.GLVersion);
		OutDeviceParameters.Add(FName(TEXT("SRC_VulkanAvailable")), AndroidProperties.VulkanAvailable ? "true" : "false");
		OutDeviceParameters.Add(FName(TEXT("SRC_VulkanVersion")), AndroidProperties.VulkanVersion);
		OutDeviceParameters.Add(FName(TEXT("SRC_AndroidVersion")), AndroidProperties.AndroidVersion);
		OutDeviceParameters.Add(FName(TEXT("SRC_DeviceMake")), AndroidProperties.DeviceMake);
		OutDeviceParameters.Add(FName(TEXT("SRC_DeviceModel")), AndroidProperties.DeviceModel);
		OutDeviceParameters.Add(FName(TEXT("SRC_DeviceBuildNumber")), AndroidProperties.DeviceBuildNumber);
		OutDeviceParameters.Add(FName(TEXT("SRC_UsingHoudini")), AndroidProperties.UsingHoudini ? "true" : "false");
		OutDeviceParameters.Add(FName(TEXT("SRC_Hardware")), AndroidProperties.Hardware);
		OutDeviceParameters.Add(FName(TEXT("SRC_Chipset")), AndroidProperties.Chipset);
		OutDeviceParameters.Add(FName(TEXT("SRC_TotalPhysicalGB")), AndroidProperties.TotalPhysicalGB);
		OutDeviceParameters.Add(FName(TEXT("SRC_HMDSystemName")), TEXT(""));
		OutDeviceParameters.Add(FName(TEXT("SRC_SM5Available")), AndroidProperties.SM5Available ? "true" : "false");
		OutDeviceParameters.Add(FAndroidProfileSelectorSourceProperties::SRC_ResolutionX, FString::Printf(TEXT("%d"), DeviceSpecs.ResolutionX));
		OutDeviceParameters.Add(FAndroidProfileSelectorSourceProperties::SRC_ResolutionY, FString::Printf(TEXT("%d"), DeviceSpecs.ResolutionY));
		OutDeviceParameters.Add(FAndroidProfileSelectorSourceProperties::SRC_InsetsLeft, FString::Printf(TEXT("%f"), DeviceSpecs.InsetsLeft));
		OutDeviceParameters.Add(FAndroidProfileSelectorSourceProperties::SRC_InsetsTop, FString::Printf(TEXT("%f"), DeviceSpecs.InsetsTop));
		OutDeviceParameters.Add(FAndroidProfileSelectorSourceProperties::SRC_InsetsRight, FString::Printf(TEXT("%f"), DeviceSpecs.InsetsRight));
		OutDeviceParameters.Add(FAndroidProfileSelectorSourceProperties::SRC_InsetsBottom, FString::Printf(TEXT("%f"), DeviceSpecs.InsetsBottom));
	}
}
#endif

const FString FAndroidDeviceProfileSelectorModule::GetDeviceProfileName()
{
	FString ProfileName; 

	// ensure SelectorProperties does actually contain our parameters
	check(FAndroidDeviceProfileSelector::GetSelectorProperties().Num() > 0);

	UE_LOG(LogAndroidDPSelector, Log, TEXT("Checking %d rules from DeviceProfile ini file."), FAndroidDeviceProfileSelector::GetNumProfiles() );
	UE_LOG(LogAndroidDPSelector, Log, TEXT("  Default profile: %s"), *ProfileName);
	for (const TTuple<FName,FString>& MapIt : FAndroidDeviceProfileSelector::GetSelectorProperties())
	{
		UE_LOG(LogAndroidDPSelector, Log, TEXT("  %s: %s"), *MapIt.Key.ToString(), *MapIt.Value);
	}

	ProfileName = FAndroidDeviceProfileSelector::FindMatchingProfile(ProfileName);

	UE_LOG(LogAndroidDPSelector, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);

	return ProfileName;
}

bool FAndroidDeviceProfileSelectorModule::GetSelectorPropertyValue(const FName& PropertyType, FString& PropertyValueOUT)
{
	if (const FString* Found = FAndroidDeviceProfileSelector::GetSelectorProperties().Find(PropertyType))
	{
		PropertyValueOUT = *Found;
		return true;
	}
	// Special case for non-existent config rule variables
	// they should return true and a value of '[null]'
	// this prevents configrule issues from throwing errors.
	if (PropertyType.ToString().StartsWith(TEXT("SRC_ConfigRuleVar[")))
	{
		PropertyValueOUT = TEXT("[null]");
		return true;
	}

	return false;
}

void FAndroidDeviceProfileSelectorModule::SetSelectorProperties(const TMap<FName, FString>& SelectorPropertiesIn)
{
	const int32 ResolutionX = FCString::Atoi(*SelectorPropertiesIn.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_ResolutionX));
	const int32 ResolutionY = FCString::Atoi(*SelectorPropertiesIn.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_ResolutionY));
	const float InsetsLeft = FCString::Atof(*SelectorPropertiesIn.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_InsetsLeft));
	const float InsetsTop = FCString::Atof(*SelectorPropertiesIn.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_InsetsTop));
	const float InsetsRight = FCString::Atof(*SelectorPropertiesIn.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_InsetsRight));
	const float InsetsBottom = FCString::Atof(*SelectorPropertiesIn.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_InsetsBottom));

	FString Orientation;
	bool bNeedPortrait = false;
	GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("Orientation"), Orientation, GEngineIni);
	if (Orientation.ToLower().Equals("portrait") || Orientation.ToLower().Equals("reverseportrait") || Orientation.ToLower().Equals("sensorportrait"))
	{
		bNeedPortrait = true;
	}

	if (bNeedPortrait)
	{
		SafeZones.X = InsetsLeft;
		SafeZones.Y = InsetsTop;
		SafeZones.Z = InsetsRight;
		SafeZones.W = InsetsBottom;
		ConstrainedAspectRatio = float(ResolutionX) / float(ResolutionY);
	}
	else
	{
		SafeZones.X = InsetsTop;
		SafeZones.Y = InsetsRight;
		SafeZones.Z = InsetsBottom;
		SafeZones.W = InsetsLeft;
		ConstrainedAspectRatio = float(ResolutionY) / float(ResolutionX);
	}
	FAndroidDeviceProfileSelector::SetSelectorProperties(SelectorPropertiesIn);
}
