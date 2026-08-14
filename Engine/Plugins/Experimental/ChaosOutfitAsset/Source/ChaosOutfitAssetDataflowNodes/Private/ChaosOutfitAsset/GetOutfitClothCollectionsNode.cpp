// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosOutfitAsset/GetOutfitClothCollectionsNode.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ChaosOutfitAsset/Outfit.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothRenderPatternFacade.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GetOutfitClothCollectionsNode)

#define LOCTEXT_NAMESPACE "ChaosGetOutfitClothCollectionsNode"

FChaosGetOutfitClothCollectionsNode::FChaosGetOutfitClothCollectionsNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Outfit);
	RegisterInputConnection(&LodIndex);
	RegisterOutputConnection(&Outfit, &Outfit);
	RegisterOutputConnection(&ClothCollections);
	RegisterOutputConnection(&NumLods);
	RegisterOutputConnection(&NumPieces);
}

void FChaosGetOutfitClothCollectionsNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;

	if (Out->IsA(&Outfit) || Out->IsA(&ClothCollections) || Out->IsA(&NumLods))
	{
		TObjectPtr<const UChaosOutfit> OutOutfit;
		if (const TObjectPtr<const UChaosOutfit>& InOutfit = GetValue(Context, &Outfit))
		{
			OutOutfit = InOutfit;
			SafeForwardInput(Context, &Outfit, &Outfit);
		}
		else
		{
			// No input Outfit, make up an empty output
			OutOutfit = NewObject<UChaosOutfit>();
			SetValue(Context, OutOutfit, &Outfit);
		}

		const int32 InLODIndex = GetValue(Context, &LodIndex);
		const int32 OutNumLods = (InLODIndex == INDEX_NONE) ? OutOutfit->GetNumLods() : FMath::Min(1, OutOutfit->GetNumLods());
		const int32 OutNumPieces = OutOutfit->GetPieces().Num();

		TArray<FManagedArrayCollection> OutClothCollections;
		OutClothCollections.Reserve(OutNumLods * OutNumPieces);

		// Get the material array
		const TArray<FSkeletalMaterial>& Materials = OutOutfit->GetMaterials();

		// Fix up the fixable object paths (in case the assets have been moved) and copy the collections
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		const TArray<TSharedRef<const FManagedArrayCollection>> SelectedClothCollections = OutOutfit->GetClothCollections(InLODIndex);

		for (int32 CollectionIndex = 0; CollectionIndex < SelectedClothCollections.Num(); ++CollectionIndex)
		{
			const TSharedRef<const FManagedArrayCollection>& ClothCollection = SelectedClothCollections[CollectionIndex];
			const TSharedRef<FManagedArrayCollection> OutClothCollection = MakeShared<FManagedArrayCollection>(*ClothCollection);
			FCollectionClothFacade ClothFacade(OutClothCollection);

			// Fix up physics asset paths
			if (!ClothFacade.GetPhysicsAssetPathName().IsEmpty() &&
				!AssetRegistryModule.Get().GetAssetByObjectPath(ClothFacade.GetPhysicsAssetPathName()).IsValid())
			{
				const UPhysicsAsset* const PhysicsAsset = OutOutfit->GetPieces()[CollectionIndex / OutNumLods].PhysicsAsset;
				const FString PhysicsAssetPathName = PhysicsAsset ? PhysicsAsset->GetPathName() : FString();

				Context.Warning(FString::Printf(TEXT("Can't find physics asset path [%s], will replace with [%s]"), *ClothFacade.GetPhysicsAssetPathName(), *PhysicsAssetPathName), this, Out);
				ClothFacade.SetPhysicsAssetPathName(PhysicsAssetPathName);
			}

			// Fix up material paths
			const int32 NumRenderPatterns = ClothFacade.GetNumRenderPatterns();
			for (int32 PatternIndex = 0; PatternIndex < NumRenderPatterns; ++PatternIndex)
			{
				FCollectionClothRenderPatternFacade RenderPatternFacade = ClothFacade.GetRenderPattern(PatternIndex);
				const FString& RenderMaterialPathName = RenderPatternFacade.GetRenderMaterialPathName();

				if (!RenderMaterialPathName.IsEmpty() &&
					!AssetRegistryModule.Get().GetAssetByObjectPath(RenderMaterialPathName).IsValid())
				{
					TArray<FString> RenderMaterialPathElements;
					RenderMaterialPathName.ParseIntoArray(RenderMaterialPathElements, TEXT("/"), true);

					// Find the closest end path name from all asset materials (currently there is no way to safely reference materials fromn the Outfit Piece)
					int32 BestMatchLength = 0;
					const FSkeletalMaterial* BestMatchMaterial = nullptr;
					for (const FSkeletalMaterial& Material : Materials)
					{
						if (Material.MaterialInterface)
						{
							TArray<FString> MaterialPathElements;
							Material.MaterialInterface->GetPathName().ParseIntoArray(MaterialPathElements, TEXT("/"), true);

							for (int32 IndexFromTheEnd = 0; IndexFromTheEnd < FMath::Min(RenderMaterialPathElements.Num(), MaterialPathElements.Num()); ++IndexFromTheEnd)
							{
								if (RenderMaterialPathElements.Last(IndexFromTheEnd) != MaterialPathElements.Last(IndexFromTheEnd))
								{
									break;
								}
								if (BestMatchLength == IndexFromTheEnd)
								{
									++BestMatchLength;
									BestMatchMaterial = &Material;
								}
							}
						}
					}
					const FString MaterialPathName = BestMatchMaterial ? BestMatchMaterial->MaterialInterface->GetPathName() : FString();

					Context.Warning(FString::Printf(TEXT("Can't find material asset path [%s], will replace with [%s]"), *RenderMaterialPathName, *MaterialPathName), this, Out);
					RenderPatternFacade.SetRenderMaterialPathName(MaterialPathName);
				}
			}

			// TODO: Fix up skeletal mesh paths, can't currently be done for 5.6 due to hotfix rules.

			// Copy collection
			OutClothCollections.Emplace(MoveTemp(*OutClothCollection));
		}
		SetValue(Context, MoveTemp(OutClothCollections), &ClothCollections);
		SetValue(Context, OutNumLods, &NumLods);
		SetValue(Context, OutNumPieces, &NumPieces);
	}
}

#undef LOCTEXT_NAMESPACE
