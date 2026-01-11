// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_CineAssembly.h"
#include "CineAssembly.h"
#include "CineAssemblyToolsStyle.h"

#include "Editor.h"
#include "FileHelpers.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

DEFINE_LOG_CATEGORY_STATIC(LogCineAssemblyDefinition, Log, All)

FText UAssetDefinition_CineAssembly::GetAssetDisplayName() const
{
	return LOCTEXT("AssetTypeActions_CineAssembly", "Cine Assembly");
}

FText UAssetDefinition_CineAssembly::GetAssetDisplayName(const FAssetData& AssetData) const
{
	const FAssetDataTagMapSharedView::FFindTagResult AssemblyType = AssetData.TagsAndValues.FindTag(UCineAssembly::AssetRegistryTag_AssemblyType);

	if (AssemblyType.IsSet())
	{
		return FText::FromString(AssemblyType.GetValue());
	}

	return GetAssetDisplayName();
}

FText UAssetDefinition_CineAssembly::GetAssetDescription(const FAssetData& AssetData) const
{
	if (const UCineAssembly* const CineAssembly = Cast<UCineAssembly>(AssetData.GetAsset()))
	{
		if (!CineAssembly->AssemblyNote.IsEmpty())
		{
			return FText::FromString(CineAssembly->AssemblyNote);
		}
	}

	return FText::GetEmpty();
}

TSoftClassPtr<> UAssetDefinition_CineAssembly::GetAssetClass() const
{
	return UCineAssembly::StaticClass();
}

FLinearColor UAssetDefinition_CineAssembly::GetAssetColor() const
{
	return FColor(229, 45, 113);
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_CineAssembly::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::Cinematics };
	return Categories;
}

const FSlateBrush* UAssetDefinition_CineAssembly::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	if (const UCineAssembly* const CineAssembly = Cast<UCineAssembly>(InAssetData.GetAsset()))
	{
		if (const UCineAssemblySchema* Schema = CineAssembly->GetSchema())
		{
			if (Schema->ThumbnailImage)
			{
				return Schema->GetThumbnailBrush();
			}
		}
	}

	return FCineAssemblyToolsStyle::Get().GetBrush("ClassThumbnail.CineAssembly");
}

EAssetCommandResult UAssetDefinition_CineAssembly::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	return Super::OpenAssets(OpenArgs);
}

TArray<FAssetData> UAssetDefinition_CineAssembly::PrepareToActivateAssets(const FAssetActivateArgs& ActivateArgs) const
{
	TArray<FAssetData> AssetsToOpen;

	// We only support opening one asset at a time
	if (ActivateArgs.Assets.Num() == 1)
	{
		const FAssetData& CineAssemblyData = ActivateArgs.Assets[0];
		AssetsToOpen.Add(CineAssemblyData);

		if (UCineAssembly* CineAssembly = Cast<UCineAssembly>(CineAssemblyData.GetAsset()))
		{
			if (CineAssembly->Level.IsValid())
			{
				if (UWorld* WorldToOpen = Cast<UWorld>(CineAssembly->Level.TryLoad()))
				{
					UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
					if (CurrentWorld != WorldToOpen)
					{
						// Prompt the user to save their unsaved changes to the current level before loading the level associated with this asset
						constexpr bool bPromptUserToSave = true;
						constexpr bool bSaveMapPackages = true;
						constexpr bool bSaveContentPackages = false;
						if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
						{
							if (!WorldToOpen->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated))
							{
								const FString FileToOpen = FPackageName::LongPackageNameToFilename(WorldToOpen->GetOutermost()->GetName(), FPackageName::GetMapPackageExtension());
								const bool bLoadAsTemplate = false;
								const bool bShowProgress = true;
								FEditorFileUtils::LoadMap(FileToOpen, bLoadAsTemplate, bShowProgress);
							}
						}
						else
						{
							// If the user canceled out of the prompt to save the current level, then do not try to open the asset
							AssetsToOpen.Empty();
						}
					}
				}
				else
				{
					UE_LOG(LogCineAssemblyDefinition, Error, TEXT("Failed to load %s while opening %s"), *CineAssembly->Level.ToString(), *CineAssembly->GetFName().ToString());
				}
			}
		}
	}

	return AssetsToOpen;
}

#undef LOCTEXT_NAMESPACE
