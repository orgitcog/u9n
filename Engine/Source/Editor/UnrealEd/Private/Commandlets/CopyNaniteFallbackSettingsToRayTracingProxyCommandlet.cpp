// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/CopyNaniteFallbackSettingsToRayTracingProxyCommandlet.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "FileHelpers.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogCopyNaniteFallbackSettingsToRayTracingProxy, Log, All);

UCopyNaniteFallbackSettingsToRayTracingProxyCommandlet::UCopyNaniteFallbackSettingsToRayTracingProxyCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UCopyNaniteFallbackSettingsToRayTracingProxyCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Display help
	if (Switches.Contains("help"))
	{
		UE_LOG(LogCopyNaniteFallbackSettingsToRayTracingProxy, Display, TEXT("CopyNaniteFallbackSettingsToRayTracingProxy"));
		UE_LOG(LogCopyNaniteFallbackSettingsToRayTracingProxy, Display, TEXT("This commandlet will copy non default Nanite Fallback Settings to the Ray Tracing Proxy Settings."));
		return 0;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	UE_LOG(LogCopyNaniteFallbackSettingsToRayTracingProxy, Display, TEXT("Searching for static meshes within the project..."));

	AssetRegistry.SearchAllAssets(true);

	TArray<FAssetData> StaticMeshAssets;
	AssetRegistry.GetAssetsByClass(UStaticMesh::StaticClass()->GetClassPathName(), StaticMeshAssets, true);

	UE_LOG(LogCopyNaniteFallbackSettingsToRayTracingProxy, Display, TEXT("Found %d static meshes"), StaticMeshAssets.Num());

	int32 Count = 0;

	for (const FAssetData& AssetData : StaticMeshAssets)
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			if (StaticMesh->NaniteSettings.FallbackTarget != ENaniteFallbackTarget::Auto && StaticMesh->RayTracingProxySettings.FallbackTarget == ENaniteFallbackTarget::Auto)
			{
				StaticMesh->RayTracingProxySettings.FallbackTarget = StaticMesh->NaniteSettings.FallbackTarget;
				StaticMesh->RayTracingProxySettings.FallbackPercentTriangles = StaticMesh->NaniteSettings.FallbackPercentTriangles;
				StaticMesh->RayTracingProxySettings.FallbackRelativeError = StaticMesh->NaniteSettings.FallbackRelativeError;

				StaticMesh->MarkPackageDirty();

				if ((++Count % 100) == 0)
				{
					UEditorLoadingAndSavingUtils::SaveDirtyPackages(/*bSaveMaps=*/false, /*bSaveAssets=*/true);
					CollectGarbage(RF_NoFlags, true);
				}
			}
		}
	}

	UEditorLoadingAndSavingUtils::SaveDirtyPackages(/*bSaveMaps=*/false, /*bSaveAssets=*/true);
	CollectGarbage(RF_NoFlags, true);

	UE_LOG(LogCopyNaniteFallbackSettingsToRayTracingProxy, Display, TEXT("Done"));

	return 0;
}

