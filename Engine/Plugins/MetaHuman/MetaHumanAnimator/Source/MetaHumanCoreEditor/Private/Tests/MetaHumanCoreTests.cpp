// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/LinkerLoad.h"
#include "Engine/SkinnedAssetCommon.h"
#include "EditorFramework/AssetImportData.h"
#include "DNAAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Editor/AssetGuideline.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanCoreTest, Verbose, All)

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMetaHumanCoreTest, "MetaHuman.Core", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FMetaHumanCoreTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FString> Tests;

	Tests.Add("AssetVersions");
	Tests.Add("AssetSize");
	Tests.Add("AssetGuidelines");

	for (const FString& Test : Tests)
	{
		OutBeautifiedNames.Add(Test);
		OutTestCommands.Add(Test);
	}
}


bool FMetaHumanCoreTest::RunTest(const FString& InTestCommand)
{
	bool bIsOK = true;

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();

	TArray<FAssetData> AssetDataList;
	constexpr bool bRecursive = true;
	constexpr bool bOnlyDiskAssets = true;
	AssetRegistry->GetAssetsByPath(TEXT("/" UE_PLUGIN_NAME), AssetDataList, bRecursive, bOnlyDiskAssets);

	if (InTestCommand == TEXT("AssetVersions"))
	{
		// Test that fails if any dependency of the MetaHuman plugin is not a plugin or engine dependency, or were created with incompatible engine version
		// This can be used to make sure we don't depend on assets that will not be available with the Marketplace build of the plugin

		// This is the latest 5.6.0 release CL
		FEngineVersionBase EngineVersionPluginTargets = FEngineVersionBase(5, 6, 0, 41862802); // TODO this is not a product version of UE 5.6.0 yet as still in development

		for (const FAssetData& AssetData : AssetDataList)
		{
			UObject* Object = AssetData.GetAsset();
			if (!Object)
			{
				UE_LOG(LogMetaHumanCoreTest, Error, TEXT("Failed to load asset '%s'"), *AssetData.PackageName.ToString());
				continue;
			}

			if (FLinkerLoad* LinkerLoad = Object->GetLinker())
			{
				// The UObject::GetLinker() will return nullptr if the asset is modified so only run the test if the linker loads can be acquired
				// In Horde this will always succeed and the test will be performed

				FEngineVersionBase EngineVersionOfAsset = LinkerLoad->EngineVer();

				const bool bIsEngineVersionOfAssetOK = (EngineVersionOfAsset.GetMajor() == EngineVersionPluginTargets.GetMajor() && EngineVersionOfAsset.GetMinor() <= EngineVersionPluginTargets.GetMinor() && EngineVersionOfAsset.GetPatch() == EngineVersionPluginTargets.GetPatch() && EngineVersionOfAsset.GetChangelist() <= EngineVersionPluginTargets.GetChangelist());
				if (!bIsEngineVersionOfAssetOK)
				{
					UE_LOG(LogMetaHumanCoreTest, Error, TEXT("Asset '%s' was serialized by engine %i.%i.%i (CL=%i). This stream of the plugin is currently set to target %i.%i.%i (CL=%i) and so Marketplace releases will be broken."), *AssetData.PackageName.ToString(), EngineVersionOfAsset.GetMajor(), EngineVersionOfAsset.GetMinor(), EngineVersionOfAsset.GetPatch(), EngineVersionOfAsset.GetChangelist(), EngineVersionPluginTargets.GetMajor(), EngineVersionPluginTargets.GetMinor(), EngineVersionPluginTargets.GetPatch(), EngineVersionPluginTargets.GetChangelist());
					continue;
				}
			}

			TArray<FAssetIdentifier> Dependencies;
			AssetRegistry->GetDependencies(AssetData.PackageName, Dependencies);

			for (const FAssetIdentifier& Dependency : Dependencies)
			{
				const FString DependencyPackageName = Dependency.PackageName.ToString();
				const bool bIsPluginOrEngineDependency = !DependencyPackageName.StartsWith(TEXT("/Game"));

				if (!bIsPluginOrEngineDependency)
				{
					UE_LOG(LogMetaHumanCoreTest, Error, TEXT("'%s' depends on '%s' which is not a plugin or engine dependency"), *AssetData.PackageName.ToString(), *DependencyPackageName);
				}
			}

			FAssetTagValueRef AssetImportTagValue = AssetData.TagsAndValues.FindTag(TEXT("AssetImportData"));
			if (AssetImportTagValue.IsSet() && AssetImportTagValue.AsString() != TEXT("[]"))
			{
				UE_LOG(LogMetaHumanCoreTest, Error, TEXT("Asset '%s' has AssetImportData tag set with value '%s'. This needs to be removed"), *AssetData.PackageName.ToString(), *AssetImportTagValue.AsString());
				continue;
			}

			// For skeletal meshes, check if there is source import data in the LODs
			if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
			{
				for (int32 LODIndex = 1; LODIndex < SkeletalMesh->GetLODNum(); ++LODIndex)
				{
					if (FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex))
					{
						if (!LODInfo->SourceImportFilename.IsEmpty())
						{
							UE_LOG(LogMetaHumanCoreTest, Error, TEXT("Skeletal Mesh '%s' has SourceImportFilename set for LOD %d with value '%s'. This needs to be removed"), *AssetData.PackageName.ToString(), LODIndex, *LODInfo->SourceImportFilename);
						}
					}
				}

				if (UDNAAsset* DNAAsset = SkeletalMesh->GetAssetUserData<UDNAAsset>())
				{
					if (UAssetImportData* AssetImportData = DNAAsset->AssetImportData)
					{
						const int32 SourceFileCount = AssetImportData->GetSourceFileCount();
						if (SourceFileCount != 0)
						{
							UE_LOG(LogMetaHumanCoreTest, Error, TEXT("Asset '%s' has %d source files in its DNAAsset user data. This needs to be removed"), *AssetData.PackageName.ToString(), SourceFileCount);
						}
					}

					if (!DNAAsset->DnaFileName.IsEmpty())
					{
						UE_LOG(LogMetaHumanCoreTest, Error, TEXT("Asset '%s' has DNAAsset user data set with DNA File Name '%s'. This needs to be removed"), *AssetData.PackageName.ToString(), *DNAAsset->DnaFileName);
					}
				}
			}
		}
	}
	else if (InTestCommand == TEXT("AssetSize"))
	{
		// Total size of assets in the plugin should currently be kept below 1.7 Gigabytes
		const double AllowedTotalAssetSizeMegabytes = 1740;
		double TotalAssetSizeMegabytes = 0;

		for (const FAssetData& AssetData : AssetDataList)
		{
			if (AssetData.IsTopLevelAsset())
			{
				UPackage* AssetPackage = AssetData.GetPackage();				
				double PackageSizeMegabytes = AssetPackage->GetFileSize() / (1024 * 1024);
				UE_LOG(LogMetaHumanCoreTest, Display, TEXT("Asset '%s' has package file size %.2f MB"), *AssetData.PackageName.ToString(), PackageSizeMegabytes);
				TotalAssetSizeMegabytes += PackageSizeMegabytes;
			}
		}

		if (TotalAssetSizeMegabytes > AllowedTotalAssetSizeMegabytes)
		{
			UE_LOG(LogMetaHumanCoreTest, Error, TEXT("Asset packages have a total file size of %.2f MB. This is higher than the current allowed total of %.2f MB. A review will be needed to keep plugin size acceptable."), TotalAssetSizeMegabytes, AllowedTotalAssetSizeMegabytes);
			bIsOK = false;
		}
		else 
		{
			UE_LOG(LogMetaHumanCoreTest, Display, TEXT("Asset packages have a total file size of %.2f MB. This is below the current allowed total of %.2f MB."), TotalAssetSizeMegabytes, AllowedTotalAssetSizeMegabytes);
		}
	}
	else if (InTestCommand == TEXT("AssetGuidelines"))
	{
		for (const FAssetData& AssetData : AssetDataList)
		{
			UObject* Object = AssetData.GetAsset();
			if (!Object)
			{
				UE_LOG(LogMetaHumanCoreTest, Error, TEXT("Failed to load asset '%s'"), *AssetData.PackageName.ToString());
				continue;
			}

			if (IInterface_AssetUserData* AssetUserDataObject = Cast<IInterface_AssetUserData>(Object))
			{
				// Check the asset does NOT contain any asset guidelines (these were removed as part of UEFN work and we dont want them accidentally reintroduced)
				const TArray<UAssetUserData*>* UserDatas = AssetUserDataObject->GetAssetUserDataArray();
				if (UserDatas)
				{
					UAssetGuideline* Guideline = nullptr;
					bIsOK &= TestFalse(TEXT("Guideline user data found"), UserDatas->FindItemByClass<UAssetGuideline>(&Guideline));
				}
			}
		}
	}
	else
	{
		bIsOK = false;
	}

	return bIsOK;
}

#endif