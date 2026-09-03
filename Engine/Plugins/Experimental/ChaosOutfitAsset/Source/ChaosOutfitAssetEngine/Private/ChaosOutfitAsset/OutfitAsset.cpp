// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosOutfitAsset/OutfitAsset.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosOutfitAsset/CollectionOutfitFacade.h"
#include "ChaosOutfitAsset/OutfitAssetPrivate.h"
#include "Dataflow/DataflowContextAssetStore.h"
#include "Engine/SkeletalMesh.h"
#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#endif
#include "Rendering/SkeletalMeshRenderData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OutfitAsset)

// If Chaos outfit asset derived data needs to be rebuilt (new format, serialization differences, etc.) replace the version GUID below with a new one.
// In case of merge conflicts with DDC versions, you MUST generate a new GUID and set this new GUID as the version.
#define UE_CHAOS_OUTFIT_ASSET_DERIVED_DATA_VERSION TEXT("DD1C25C90FDE4287881A8759CD3646A6")

UChaosOutfitAsset::UChaosOutfitAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DataflowInstance.SetDataflowTerminal(TEXT("OutfitAssetTerminal"));
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Init an empty asset with a root bone and empty but initialized render data
	Build(nullptr);
}

UChaosOutfitAsset::UChaosOutfitAsset(FVTableHelper& Helper)
	: Super(Helper)
{
}

UChaosOutfitAsset::~UChaosOutfitAsset() = default;

bool UChaosOutfitAsset::HasValidClothSimulationModels() const
{
	for (const FChaosOutfitPiece& Piece : Pieces)
	{
		if (Piece.ClothSimulationModel->GetNumLods())
		{
			return true;
		}
	}
	return false;
}

void UChaosOutfitAsset::Build(const TObjectPtr<const UChaosOutfit> InOutfit, UE::Dataflow::IContextAssetStoreInterface* ContextAssetStore)
{
	using namespace UE::Chaos::OutfitAsset;

	// Stop the rendering
	ReleaseResources();

	// Copy the outfit to this asset
	TUniquePtr<FSkeletalMeshRenderData> TempSkeletalMeshRenderData = MakeUnique<FSkeletalMeshRenderData>();
	FReferenceSkeleton TempReferenceSkeleton;

	if (InOutfit)
	{
		InOutfit->CopyTo(
			Pieces,
			TempReferenceSkeleton,
			TempSkeletalMeshRenderData,
			Materials,
			OutfitCollection);

#if WITH_EDITORONLY_DATA
		if (Outfit != InOutfit)
		{
			const FName UniqueOutfitName = MakeUniqueObjectName(this, UChaosOutfit::StaticClass());
			Outfit = DuplicateObject<UChaosOutfit>(InOutfit, this, UniqueOutfitName);
		}
#endif
		if (ContextAssetStore)
		{
			// Fix up resized material paths
			TMap<FString, FString> MaterialPathsToFixUp;
			MaterialPathsToFixUp.Reserve(Materials.Num());

			for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
			{
				FSkeletalMaterial& Material = Materials[MaterialIndex];
				if (Material.MaterialInterface && Material.MaterialInterface->GetOuter() == GetTransientPackage())
				{
					const FString TransientPathName = Material.MaterialInterface->GetPathName();
					Material.MaterialInterface = Cast<UMaterialInterface>(ContextAssetStore->CommitAsset(TransientPathName));
#if WITH_EDITORONLY_DATA
					if (Outfit)
					{
						Outfit->GetMaterials()[MaterialIndex].MaterialInterface = Material.MaterialInterface;
					}
#endif
					MaterialPathsToFixUp.Emplace(TransientPathName, Material.MaterialInterface ? Material.MaterialInterface->GetPathName() : FString());
				}
			}

			// Fix up cloth collections
			if (MaterialPathsToFixUp.Num())
			{
				auto FixUpPiecesMaterials = [&MaterialPathsToFixUp](TArrayView<FChaosOutfitPiece> PiecesToFixUp)
					{
						using namespace UE::Chaos::ClothAsset;
						for (FChaosOutfitPiece& Piece : PiecesToFixUp)
						{
							for (TSharedRef<const FManagedArrayCollection>& Collection : Piece.Collections)
							{
								TOptional<TSharedRef<FManagedArrayCollection>> FixedUpCollection;
								TOptional<FCollectionClothFacade> FixedUpClothFacade;

								FCollectionClothConstFacade ClothFacade(Collection);
								const TConstArrayView<FString> RenderMaterialPathNames = ClothFacade.GetRenderMaterialPathName();
								for (int32 PathIndex = 0; PathIndex < RenderMaterialPathNames.Num(); ++PathIndex)
								{
									const FString& RenderMaterialPathName = RenderMaterialPathNames[PathIndex];
									if (const FString* MaterialPathToFixUp = MaterialPathsToFixUp.Find(RenderMaterialPathName))
									{
										if (!FixedUpCollection.IsSet())
										{
											FixedUpCollection.Emplace(MakeShared<FManagedArrayCollection>(*Collection));
											FixedUpClothFacade.Emplace(FixedUpCollection.GetValue());
										}
										TArrayView<FString> FixedUpRenderMaterialPathNames = FixedUpClothFacade->GetRenderMaterialPathName();
										check(FixedUpRenderMaterialPathNames[PathIndex] == RenderMaterialPathName);
							
										FixedUpRenderMaterialPathNames[PathIndex] = *MaterialPathToFixUp;
									}
								}

								if (FixedUpCollection.IsSet())
								{
									Collection = MoveTemp(FixedUpCollection.GetValue());
								}
							}
						}
					};

				// Fix up this outfit's pieces
				FixUpPiecesMaterials(Pieces);

#if WITH_EDITORONLY_DATA
				// Fix up this outfit construction's pieces
				if (Outfit)
				{
					FixUpPiecesMaterials(Outfit->GetPieces());
				}
#endif
			}
		}
	}
	else
	{
		UChaosOutfit::Init(
			Pieces,
			TempReferenceSkeleton,
			TempSkeletalMeshRenderData,
			Materials,
			OutfitCollection);

#if WITH_EDITORONLY_DATA
		Outfit = nullptr;
#endif
	}

	// Populate the body sizes
	const FCollectionOutfitConstFacade OutfitFacade(OutfitCollection);
	const TArray<FString> OutfitBodyPartsSkeletalMeshes = OutfitFacade.GetOutfitBodyPartsSkeletalMeshes();
	Bodies.Reset(OutfitBodyPartsSkeletalMeshes.Num());
	for (const FString& OutfitBodyPartsSkeletalMesh : OutfitBodyPartsSkeletalMeshes)
	{
		if (USkeletalMesh* const Body = LoadObject<USkeletalMesh>(this, *OutfitBodyPartsSkeletalMesh))
		{
			Bodies.Emplace(Body);
		}
	}

	// Set the new reference skeleton
	SetReferenceSkeleton(&TempReferenceSkeleton);
	CalculateInvRefMatrices();

	// Create the render data
	if (!TempSkeletalMeshRenderData->LODRenderData.Num())
	{
		TempSkeletalMeshRenderData->LODRenderData.Add(new FSkeletalMeshLODRenderData());  // Always needs at least one LOD when rendering
		TempSkeletalMeshRenderData->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer.Init(0);  //  Required for serialization
		TempSkeletalMeshRenderData->LODRenderData[0].StaticVertexBuffers.StaticMeshVertexBuffer.Init(0, 0);  //  Required for serialization
	}
	SetResourceForRendering(MoveTemp(TempSkeletalMeshRenderData));

	CalculateBounds();

	const int32 NumLODs = GetResourceForRendering()->LODRenderData.Num();  // The render data will always look for at least one default LOD 0
	LODInfo.Reset(NumLODs);
	LODInfo.AddDefaulted(NumLODs);

	if (FApp::CanEverRender())
	{
		InitResources();
	}

	// Re-register any components using this asset to restart the simulation and update the leader bone map
	ReregisterComponents();
}

void UChaosOutfitAsset::Serialize(FArchive& Ar)
{
	LLM_SCOPE_BYNAME(TEXT("Physics/Cloth"));
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (Outfit)
#endif
	{
		bool bCooked = Ar.IsCooking();
		Ar << bCooked;

		if (bCooked && !IsTemplate() && !Ar.IsCountingMemory())  // Counting of these resources are done in GetResourceSizeEx, so skip these when counting memory
		{
			LLM_SCOPE_BYNAME(TEXT("Physics/ClothRendering"));
			if (Ar.IsLoading())
			{
				SetResourceForRendering(MakeUnique<FSkeletalMeshRenderData>());
			}
			GetResourceForRendering()->Serialize(Ar, this);
		}
	}
}

void UChaosOutfitAsset::PostLoad()
{
	LLM_SCOPE_BYNAME(TEXT("Physics/Cloth"));
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (Outfit)
	{
		// Rebuild the outfit
		Build(Outfit);
	}
	else
	{
		// Re-evaluate the Dataflow (legacy PostLoad behavior from before the Outfit object was saved)
		GetDataflowInstance().UpdateOwnerAsset();
		if (UPackage* const Package = GetOutermost())
		{
			Package->SetDirtyFlag(true);
		}
		UE_LOG(LogChaosOutfitAsset, Warning, TEXT("Outfit Asset [%s] needs to be re-saved."), *GetName());
	}
#endif

	if (FApp::CanEverRender())
	{
		InitResources();
	}
	else
	{
		UpdateUVChannelData(false);  // Update any missing data when cooking
	}

	CalculateInvRefMatrices();
	CalculateBounds();
}

FName UChaosOutfitAsset::GetClothSimulationModelName(int32 ModelIndex) const
{
	return Pieces[ModelIndex].Name;
}

TSharedPtr<const FChaosClothSimulationModel> UChaosOutfitAsset::GetClothSimulationModel(int32 ModelIndex) const
{
	return Pieces[ModelIndex].ClothSimulationModel;
}

const TArray<TSharedRef<const FManagedArrayCollection>>& UChaosOutfitAsset::GetCollections(int32 ModelIndex) const
{
	return Pieces[ModelIndex].Collections;
}

const UPhysicsAsset* UChaosOutfitAsset::GetPhysicsAssetForModel(int32 ModelIndex) const
{
	return Pieces[ModelIndex].PhysicsAsset;
}

FGuid UChaosOutfitAsset::GetAssetGuid(int32 ModelIndex) const
{
	return Pieces[ModelIndex].AssetGuid;
}

#if WITH_EDITOR
FString UChaosOutfitAsset::BuildDerivedDataKey(const ITargetPlatform* TargetPlatform)
{
	const FString KeySuffix;
	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("CHAOSOUTFIT"),
		UE_CHAOS_OUTFIT_ASSET_DERIVED_DATA_VERSION,
		*KeySuffix
	);
}
#endif // #if WITH_EDITOR

void UChaosOutfitAsset::CalculateBounds()
{
	FBox BoundingBox(ForceInit);
	for (const FSkeletalMeshLODRenderData& LODRenderDatum : GetResourceForRendering()->LODRenderData)
	{
		const FPositionVertexBuffer& PositionVertexBuffer = LODRenderDatum.StaticVertexBuffers.PositionVertexBuffer;

		for (uint32 VertexIndex = 0; VertexIndex < PositionVertexBuffer.GetNumVertices(); ++VertexIndex)
		{
			const FVector3f& VertexPosition = PositionVertexBuffer.VertexPosition(VertexIndex);
			BoundingBox += (FVector)VertexPosition;
		}
	}
	Bounds = FBoxSphereBounds(BoundingBox);
}
