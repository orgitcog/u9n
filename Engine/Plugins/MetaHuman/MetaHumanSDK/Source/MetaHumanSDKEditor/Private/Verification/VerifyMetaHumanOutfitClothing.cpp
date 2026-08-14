// Copyright Epic Games, Inc. All Rights Reserved.


#include "Verification/VerifyMetaHumanOutfitClothing.h"

#include "MetaHumanAssetReport.h"
#include "Verification/MetaHumanCharacterVerification.h"
#include "Verification/VerifyMetaHumanSkeletalClothing.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/Paths.h"
#include "Misc/RuntimeErrors.h"

#define LOCTEXT_NAMESPACE "VerifyMetaHumanOutfitClothing"

namespace UE::MetaHuman::Private
{
void VerifyWardrobeItem(const UObject* OutfitAsset, UMetaHumanAssetReport* Report)
{
	FString RootFolder = FPaths::GetPath(OutfitAsset->GetPathName());

	TArray<FAssetData> TopLevelItems;
	IAssetRegistry::GetChecked().GetAssetsByPath(FName(RootFolder), TopLevelItems);

	bool bWardrobeItemFound = false;

	for (const FAssetData& Item : TopLevelItems)
	{
		if (FPaths::GetBaseFilename(Item.PackageName.ToString()).StartsWith(TEXT("WI_")))
		{
			bWardrobeItemFound = true;
			FMetaHumanCharacterVerification::Get().VerifyOutfitWardrobeItem(Item.GetAsset(), OutfitAsset, Report);
		}
	}

	// 2008 Check for MetaHuman Wardrobe Item per asset
	if (!bWardrobeItemFound)
	{
		Report->AddWarning({LOCTEXT("MissingWardrobeItem", "The package does not contain a Wardrobe Item. Certain features will not work or will be at default values")});
	}
}
}

void UVerifyMetaHumanOutfitClothing::Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const
{
	using namespace UE::MetaHuman::Private;
	if (!ensureAsRuntimeWarning(ToVerify) || !ensureAsRuntimeWarning(Report))
	{
		return;
	}

	FString RootFolder = FPaths::GetPath(ToVerify->GetPathName());

	TArray<FAssetData> TopLevelItems;
	IAssetRegistry::GetChecked().GetAssetsByPath(FName(RootFolder), TopLevelItems);

	for (const FAssetData& Item : TopLevelItems)
	{
		if (FPaths::GetBaseFilename(Item.PackageName.ToString()).StartsWith(TEXT("WI_")))
		{
			UE::MetaHuman::FMetaHumanCharacterVerification::Get().VerifyOutfitWardrobeItem(Item.GetAsset(), ToVerify, Report);
		}
	}

	// Verify the structure of the OutfitAsset itself (need to defer implementation to another module due to dependencies)
	UE::MetaHuman::FMetaHumanCharacterVerification::Get().VerifyOutfitAsset(ToVerify, Report);

	// Check any wardrobe items that are present
	VerifyWardrobeItem(ToVerify, Report);

	// Verify that all clothing assets in the package are compatible
	UVerifyMetaHumanSkeletalClothing::VerifyClothingCompatibleAssets(ToVerify, Report);
}

#undef LOCTEXT_NAMESPACE
