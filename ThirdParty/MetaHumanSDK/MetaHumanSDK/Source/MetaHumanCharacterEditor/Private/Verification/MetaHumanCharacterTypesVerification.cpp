// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterTypesVerification.h"

#include "Item/MetaHumanGroomPipeline.h"
#include "Item/MetaHumanOutfitPipeline.h"
#include "Item/MetaHumanSkeletalMeshPipeline.h"
#include "MetaHumanItemPipeline.h"
#include "MetaHumanWardrobeItem.h"

#include "ChaosOutfitAsset/OutfitAsset.h"
#include "ChaosOutfitAsset/SizedOutfitSource.h"
#include "GroomBindingAsset.h"
#include "MetaHumanAssetReport.h"
#include "MetaHumanItemEditorPipeline.h"
#include "ProjectUtilities/MetaHumanAssetManager.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterTypesVerification"

namespace UE::MetaHuman::Private
{
const UMetaHumanWardrobeItem* GetWardrobeItem(TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report)
{
	const UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(Target);
	if (!WardrobeItem)
	{
		Report->AddError({FText::Format(LOCTEXT("InvalidWardrobeItem", "The object {0} is not a valid Wardrobe Item"), FText::FromName(Target->GetFName()))});
	}
	return WardrobeItem;
}

void CheckPrincipalItem(TNotNull<const UMetaHumanWardrobeItem*> WardrobeItem, TNotNull<const UObject*> MainAsset, UMetaHumanAssetReport* Report)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("WardrobeItemName"), FText::FromString(WardrobeItem->GetName()));
	Args.Add(TEXT("MainAssetName"), FText::FromString(MainAsset->GetName()));

	if (WardrobeItem->PrincipalAsset.Get() != MainAsset)
	{
		Report->AddError({FText::Format(LOCTEXT("WardrobeItemDoesNotReferencePrincipalAsset", "The WardrobeItem {WardrobeItemName} does not reference {MainAssetName} as its Principal Asset."), Args)});
	}
}
}

void UMetaHumanCharacterTypesVerification::VerifyGroomWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> GroomBindingAsset, UMetaHumanAssetReport* Report)
{
	using namespace UE::MetaHuman::Private;

	const UMetaHumanWardrobeItem* WardrobeItem = GetWardrobeItem(Target, Report);
	if (!WardrobeItem)
	{
		return;
	}

	check(Cast<UGroomBindingAsset>(GroomBindingAsset));

	CheckPrincipalItem(WardrobeItem, GroomBindingAsset, Report);

	const TObjectPtr<const UMetaHumanItemPipeline> Pipeline = WardrobeItem->GetPipeline();
	if (Pipeline != nullptr && !Pipeline->GetClass()->IsChildOf(UMetaHumanGroomPipeline::StaticClass()))
	{
		Report->AddError({FText::Format(LOCTEXT("IncorrectGroomPipeline", "The WardrobeItem {0} should use a pipeline derived from UMetaHumanGroomPipeline or the default pipeline."), FText::FromName(WardrobeItem->GetFName()))});
	}
}
void UMetaHumanCharacterTypesVerification::VerifySkelMeshClothingWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> SkeletalMesh, UMetaHumanAssetReport* Report)
{
	using namespace UE::MetaHuman::Private;

	const UMetaHumanWardrobeItem* WardrobeItem = GetWardrobeItem(Target, Report);
	if (!WardrobeItem)
	{
		return;
	}

	check(Cast<USkeletalMesh>(SkeletalMesh));

	CheckPrincipalItem(WardrobeItem, SkeletalMesh, Report);

	const TObjectPtr<const UMetaHumanItemPipeline> Pipeline = WardrobeItem->GetPipeline();
	if (Pipeline != nullptr && !Pipeline->GetClass()->IsChildOf(UMetaHumanSkeletalMeshPipeline::StaticClass()))
	{
		Report->AddError({FText::Format(LOCTEXT("IncorrectSkelMeshPipeline", "The WardrobeItem {0} should use a pipeline derived from UMetaHumanSkeletalMeshPipeline or the default pipeline."), FText::FromName(WardrobeItem->GetFName()))});
	}
}
void UMetaHumanCharacterTypesVerification::VerifyOutfitWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> OutfitAsset, UMetaHumanAssetReport* Report)
{
	using namespace UE::MetaHuman::Private;

	const UMetaHumanWardrobeItem* WardrobeItem = GetWardrobeItem(Target, Report);
	if (!WardrobeItem)
	{
		return;
	}

	check(Cast<UChaosOutfitAsset>(OutfitAsset));

	CheckPrincipalItem(WardrobeItem, OutfitAsset, Report);

	const TObjectPtr<const UMetaHumanItemPipeline> Pipeline = WardrobeItem->GetPipeline();
	if (Pipeline != nullptr && !Pipeline->GetClass()->IsChildOf(UMetaHumanOutfitPipeline::StaticClass()))
	{
		Report->AddError({FText::Format(LOCTEXT("IncorrectOutfitPipeline", "The WardrobeItem {0} should use a pipeline derived from UMetaHumanOutfitPipeline or the default pipeline."), FText::FromName(WardrobeItem->GetFName()))});
	}
}

void UMetaHumanCharacterTypesVerification::VerifyOutfitAsset(TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("TargetName"), FText::FromName(Target->GetFName()));

	const UChaosOutfitAsset* OutfitAsset = Cast<UChaosOutfitAsset>(Target);
	if (!OutfitAsset)
	{
		Report->AddError({FText::Format(LOCTEXT("AssetIsNotAnOutfitAsset", "The Asset {TargetName} is not a valid Outfit Asset."), Args)});
		return;
	}

	// 2006 All sizes have corresponding bodies
	TValueOrError<const FPropertyBagArrayRef, EPropertyBagResult> Result = OutfitAsset->GetDataflowInstance().GetVariables().GetArrayRef(TEXT("SizedOutfitSource"));

	if (Result.HasValue())
	{
		for (int32 SourceIndex = 0; SourceIndex < Result.GetValue().Num(); SourceIndex++)
		{
			Args.Add(TEXT("SourceIndex"), SourceIndex);
			const TValueOrError<FStructView, EPropertyBagResult> ItemResult = Result.GetValue().GetValueStruct(SourceIndex);
			if (ItemResult.HasValue())
			{
				FChaosSizedOutfitSource& OutfitSource = ItemResult.GetValue().Get<FChaosSizedOutfitSource>();
				if (OutfitSource.SourceBodyParts.IsEmpty())
				{
					Report->AddError({FText::Format(LOCTEXT("OutfitAssetMissingBodyParts", "The Asset {TargetName} is missing body parts for Sized Outfit Source {SourceIndex}."), Args)});
				}
				else
				{
					// 2007 Using garment construction bodies
					if (!OutfitSource.SourceBodyParts[0].GetName().EndsWith("CombinedSkelMesh"))
					{
						Report->AddWarning({FText::Format(LOCTEXT("OutfitAssetBodyPartsNotComplete", "Sized Outfit Source {SourceIndex} for {TargetName} may not be using a combined skel mesh (Body and Head)."), Args)});
					}

					// 2009 Source body is a full body metahuman (topo matches as well as skeleton)
				}
			}
		}
	}

	// Using garment construction bodies
}

void UMetaHumanCharacterTypesVerification::VerifyMetaHumanCharacterAsset(TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report)
{
}

FClothingAssetDetails UMetaHumanCharacterTypesVerification::GetDetailsForClothingAsset(TNotNull<const UObject*> Target)
{
	FClothingAssetDetails Details;
	if (const UChaosOutfitAsset* OutfitAsset = Cast<UChaosOutfitAsset>(Target))
	{
		TValueOrError<const FPropertyBagArrayRef, EPropertyBagResult> Result = OutfitAsset->GetDataflowInstance().GetVariables().GetArrayRef(TEXT("SizedOutfitSource"));

		if (Result.HasValue() && Result.GetValue().Num())
		{
			Details.bResizesWithBlendableBodies = true;
		}
	}

	UPackage* Package = Target->GetPackage();
	check(Package);
	FName WardrobeItemPackage = UMetaHumanAssetManager::GetWardrobeItemPackage(Package->GetFName());
	if (UMetaHumanWardrobeItem* WardrobeItem = LoadObject<UMetaHumanWardrobeItem>(nullptr, *WardrobeItemPackage.ToString()))
	{
		if(const UMetaHumanItemEditorPipeline* EditorPipeline = WardrobeItem->GetEditorPipeline())
		{
			// Use reflection to avoid a circular dependency on MetaHumanDefaultEditorPipeline.
			if (FProperty* BodyHiddenFaceMap = EditorPipeline->GetClass()->FindPropertyByName(TEXT("BodyHiddenFaceMap")))
			{
				UTexture2D* Value = nullptr;
				BodyHiddenFaceMap->GetValue_InContainer(EditorPipeline, &Value);
				if (Value)
				{
					Details.bHasClothingMask = true;
				}
			}
		}
	}
	return Details;
}

#undef LOCTEXT_NAMESPACE
