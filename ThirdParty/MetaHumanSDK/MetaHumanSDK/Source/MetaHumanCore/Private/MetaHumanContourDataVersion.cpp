// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanContourDataVersion.h"
#include "Serialization/CustomVersion.h"
#include "MetaHumanCoreLog.h"
#include "Misc/EngineVersion.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const FString FMetaHumanContourDataVersion::ConfigFileName = TEXT("curves_config.json");

FString FMetaHumanContourDataVersion::GetContourDataVersionString()
{
	FString VersionString;
	FString JsonString;
	const FString JsonFilePath = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir() +
		"/MeshFitting/Template/" + FMetaHumanContourDataVersion::ConfigFileName;

	FFileHelper::LoadFileToString(JsonString, *JsonFilePath);
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		TMap<FString, TSharedPtr<FJsonValue>> TopLevelValues = JsonObject->Values;

		if (TSharedPtr<FJsonValue>* ConfigDataEntry = TopLevelValues.Find("version"))
		{
			TSharedPtr<FJsonObject> CurrentObject = (*ConfigDataEntry)->AsObject();
			uint16 MajorVersion = 0; uint16 MinorVersion = 0;
			
			CurrentObject->TryGetNumberField(TEXT("major"), MajorVersion);
			CurrentObject->TryGetNumberField(TEXT("minor"), MinorVersion);

			const FEngineVersion ContourDataVersionAsEngineVersion = FEngineVersion{ MajorVersion, MinorVersion, 0, 0, TEXT("") };
			VersionString = ContourDataVersionAsEngineVersion.ToString(EVersionComponent::Patch);
		}
		else
		{
			UE_LOG(LogMetaHumanCore, Error, TEXT("Unable to retrieve a version from curves_config.json. The config does not contain the version filed"));
		}
	}

	return VersionString;
}

bool FMetaHumanContourDataVersion::CheckVersionCompatibility(const TArray<FString>& InVersionStringList, ECompatibilityResult& OutResult)
{
	bool bVersionCompatible = InVersionStringList.IsEmpty();
	OutResult = ECompatibilityResult::NoUpgrade;

	FEngineVersion ConfigVersion;
	FEngineVersion::Parse(GetContourDataVersionString(), ConfigVersion);
	
	for (const FString& VersionString : InVersionStringList)
	{
		FEngineVersion CheckedVersion;
		FString PromotedContourDataVersion = VersionString.IsEmpty() ? "0.0.0" : VersionString;
		// Try to parse the version from the string
		if (FEngineVersion::Parse(PromotedContourDataVersion, CheckedVersion))
		{
			// Compare MeshTracker version in the asset with current version in the editor
			EVersionComponent VersionComponent;
			EVersionComparison VersionComparison = FEngineVersionBase::GetNewest(ConfigVersion, CheckedVersion, &VersionComponent);
			if (EVersionComparison::First == VersionComparison)
			{
				// By default, treat major upgrades as incompatible
				if (VersionComponent == EVersionComponent::Major)
				{
					OutResult = ECompatibilityResult::NeedsUpgrade;
					return false;
				}
				else if (VersionComponent == EVersionComponent::Minor)
				{
					OutResult = ECompatibilityResult::AutoUpgrade;
					bVersionCompatible = true;
				}
			}
			else if (EVersionComparison::Second == VersionComparison)
			{
				return false;
			}
			else if (EVersionComparison::Neither == VersionComparison)
			{
				bVersionCompatible = true;
			}
		}
	}

	return bVersionCompatible;
}
