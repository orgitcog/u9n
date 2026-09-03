// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Async/ParallelFor.h"
#include "AsyncLoadingTests_Shared.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/LinkerLoad.h"
#include "UObject/UObjectHash.h"

#if WITH_DEV_AUTOMATION_TESTS

#undef TEST_NAME_ROOT
#define TEST_NAME_ROOT "System.Engine.Loading"

/**
 * This test demonstrate that LoadPackageAsync is thread-safe and can be called from multiple workers at the same time.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThreadSafeAsyncLoadingTest, TEXT(TEST_NAME_ROOT ".ThreadSafeAsyncLoadingTest"), EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FThreadSafeAsyncLoadingTest::RunTest(const FString& Parameters)
{
	// We use the asset registry to get a list of asset to load. 
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName(TEXT("AssetRegistry"))).Get();
	AssetRegistry.WaitForCompletion();

	// Limit the number of packages we're going to load for the test in case the project is very big.
	constexpr int32 MaxPackageCount = 5000;

	TSet<FName> UniquePackages;
	AssetRegistry.EnumerateAllAssets(
		[&UniquePackages](const FAssetData& AssetData)
		{
			if (UniquePackages.Num() < MaxPackageCount)
			{
				if (LoadingTestsUtils::IsAssetSuitableForTests(AssetData))
				{
					UniquePackages.FindOrAdd(AssetData.PackageName);
				}

				return true;
			}
			
			return false;
		},
		UE::AssetRegistry::EEnumerateAssetsFlags::OnlyOnDiskAssets
	);

	TArray<FName> PackagesToLoad(UniquePackages.Array());
	TArray<int32> RequestIDs;
	RequestIDs.SetNum(PackagesToLoad.Num());

	ParallelFor(PackagesToLoad.Num(),
		[&PackagesToLoad, &RequestIDs](int32 Index)
		{
			RequestIDs[Index] = LoadPackageAsync(PackagesToLoad[Index].ToString());
		}
	);

	FlushAsyncLoading(RequestIDs);

	return true;
}

/**
 * Ensure we can properly handle Serialize implementations that might invalidate exports during preload.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncLoadingTestInvalidateExportDuringPreload, TEXT(TEST_NAME_ROOT ".InvalidateExportDuringPreload"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAsyncLoadingTestInvalidateExportDuringPreload::RunTest(const FString& Parameters)
{
	auto VerifyLoad = [this](FLoadingTestsScope& LoadingTestScope, bool bExpectToFindObject)
		{
			UPackage* Package = LoadPackage(nullptr, LoadingTestScope.PackagePath1, LOAD_None);
			TestTrue(TEXT("The pacakge should load successfully"), Package != nullptr);
			
			// Exclude garbage objects as the GC won't have run yet but invalidated objects shdould be marked as garbage by this point
			UAsyncLoadingTests_Shared* Object1 = FindObjectFast<UAsyncLoadingTests_Shared>(Package, LoadingTestScope.ObjectName, true/*bExactClass*/, EObjectFlags::RF_MirroredGarbage);
			if (bExpectToFindObject)
			{
				TestTrue(TEXT("The object should have been loaded"), Object1 != nullptr);
			}
			else
			{
				TestTrue(TEXT("The object should not have been loaded"), Object1 == nullptr);
			}
		};

	{
		FLoadingTestsScope LoadingTestScope(this);


		VerifyLoad(LoadingTestScope, true /*bExpectToFindObject*/);
	}
	
	{
		FLoadingTestsScope LoadingTestScope(this);

		UAsyncLoadingTests_Shared::OnSerialize.BindLambda(
			[this](FArchive& Ar, UAsyncLoadingTests_Shared* Object)
			{
				if (Ar.IsLoading())
				{
					if (FLinkerLoad* Linker = Object->GetLinker())
					{
						Object->MarkAsGarbage();
						Linker->InvalidateExport(Object, true /*bHidesGarbageObjects*/);
					}
				}
			}
		);

		VerifyLoad(LoadingTestScope, false /*bExpectToFindObject*/);
	}
	return true;
}

#undef TEST_NAME_ROOT
#endif // WITH_DEV_AUTOMATION_TESTS
