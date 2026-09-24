// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothAssetBuilder.h"
#include "ChaosClothAsset/ClothAssetPrivate.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "Chaos/CollectionEmbeddedSpringConstraintFacade.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Animation/Skeleton.h"
#if WITH_EDITORONLY_DATA
#include "Animation/AnimationAsset.h"
#endif
#include "Engine/RendererSettings.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetAsyncCompileUtils.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/Package.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "IMeshBuilderModule.h"
#include "DerivedDataCacheInterface.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothAsset)

// If Chaos cloth asset derived data needs to be rebuilt (new format, serialization differences, etc.) replace the version GUID below with a new one. 
// In case of merge conflicts with DDC versions, you MUST generate a new GUID and set this new GUID as the version.
#define CHAOS_CLOTH_ASSET_DERIVED_DATA_VERSION TEXT("479D81081F3A4A22B3C22ED4B278680E")

#define LOCTEXT_NAMESPACE "ChaosClothAsset"

namespace UE::Chaos::ClothAsset::Private
{
	static bool bClothCollectionOnlyCookRequiredFacades = true;

	FAutoConsoleVariableRef CVarClothCollectionOnlyCookRequiredFacades(
		TEXT("p.ClothCollectionOnlyCookRequiredFacades"),
		bClothCollectionOnlyCookRequiredFacades,
		TEXT("Default setting for culling managed arrays on the cloth collection during the cook. Default[true]"));

	static bool HasValidSkinweights(const TConstArrayView<TArray<int32>> BoneIndices, TConstArrayView<TArray<float>> BoneWeights, const FReferenceSkeleton* RefSkeleton)
	{
		if (!RefSkeleton)
		{
			return false;
		}

		check(BoneIndices.Num() == BoneWeights.Num());
		for (int32 Index = 0; Index < BoneIndices.Num(); ++Index)
		{
			if (!BoneIndices[Index].Num() || !BoneWeights[Index].Num() || BoneIndices[Index].Num() != BoneWeights[Index].Num())
			{
				return false;
			}
			for (const int32 BoneIndex : BoneIndices[Index])
			{
				if (!RefSkeleton->IsValidIndex(BoneIndex))
				{
					return false;
				}
			}

		}
		return true;
	}

	static ::Chaos::FChaosArchive& Serialize(::Chaos::FChaosArchive& Ar, TArray<TSharedRef<const FManagedArrayCollection>>& ClothCollections)
	{
		Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

		if (Ar.IsLoading() && Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::ClothCollectionSingleLodSchema)
		{
			// Cloth assets before this version had a single ClothCollection with a completely different schema.
			ClothCollections.Empty(1);
			TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>();
			ClothCollection->Serialize(Ar);

			// Now we're just going to hard reset and define a new schema.
			ClothCollection->Reset();
			FCollectionClothFacade ClothFacade(ClothCollection);
			ClothFacade.DefineSchema();

			ClothCollections.Emplace(MoveTemp(ClothCollection));

			return Ar;
		}
		else
		{
			// This is following Serialize for Arrays
			ClothCollections.CountBytes(Ar);
			int32 SerializeNum = Ar.IsLoading() ? 0 : ClothCollections.Num();
			Ar << SerializeNum;
			if (SerializeNum == 0)
			{
				// if we are loading, then we have to reset the size to 0, in case it isn't currently 0
				if (Ar.IsLoading())
				{
					ClothCollections.Empty();
				}
				return Ar;
			}
			check(SerializeNum >= 0);

			if (Ar.IsError() || SerializeNum < 0)
			{
				Ar.SetError();
				return Ar;
			}
			if (Ar.IsLoading())
			{
				// Required for resetting ArrayNum
				ClothCollections.Empty(SerializeNum);

				for (int32 i = 0; i < SerializeNum; i++)
				{
					TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>();
					ClothCollection->Serialize(Ar);
					ClothCollections.Emplace(MoveTemp(ClothCollection));
				}
			}
			else
			{
				check(SerializeNum == ClothCollections.Num());

				for (int32 i = 0; i < SerializeNum; i++)
				{
					ConstCastSharedRef<FManagedArrayCollection>(ClothCollections[i])->Serialize(Ar);
				}
			}

			return Ar;
		}
	}

	static TArray<TSharedRef<const FManagedArrayCollection>> TrimOnCook(const FString InAssetName, const TArray<TSharedRef<const FManagedArrayCollection>>& InClothCollections)
	{
		int32 Index = 0;
#if WITH_EDITORONLY_DATA
		if (bClothCollectionOnlyCookRequiredFacades)
		{
			TArray<TSharedRef<const FManagedArrayCollection>> OutputCollections;
			for (TSharedRef<const FManagedArrayCollection> ClothCollection : InClothCollections)
			{
				// Properties
				TSharedRef<FManagedArrayCollection> PropertyCollection = MakeShared<FManagedArrayCollection>();
				::Chaos::Softs::FCollectionPropertyMutableFacade CollectionPropertyMutableFacade(PropertyCollection);
				CollectionPropertyMutableFacade.Copy(*ClothCollection);

				// Springs
				const ::Chaos::Softs::FEmbeddedSpringFacade InEmbeddedSpringFacade(ClothCollection.Get(), UE::Chaos::ClothAsset::ClothCollectionGroup::SimVertices3D);
				if (InEmbeddedSpringFacade.IsValid())
				{
					::Chaos::Softs::FEmbeddedSpringFacade EmbeddedSpringFacade(*PropertyCollection, ClothCollectionGroup::SimVertices3D);
					EmbeddedSpringFacade.DefineSchema();
					constexpr int32 VertexOffset = 0;
					EmbeddedSpringFacade.Append(InEmbeddedSpringFacade, VertexOffset);
				}

				// Morph targets
				const UE::Chaos::ClothAsset::FCollectionClothConstFacade InClothFacade(ClothCollection);
				if (InClothFacade.IsValid(UE::Chaos::ClothAsset::EClothCollectionExtendedSchemas::CookedOnly))
				{
					UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(PropertyCollection);
					ClothFacade.DefineSchema(UE::Chaos::ClothAsset::EClothCollectionExtendedSchemas::CookedOnly);
					ClothFacade.InitializeCookedOnly(InClothFacade);
				}

				OutputCollections.Emplace(MoveTemp(PropertyCollection));

				UE_LOG(LogChaosClothAsset, Display, TEXT("TrimOnCook[ON] %s:[%d] [size:%d]"), 
					*InAssetName, Index++, PropertyCollection->GetAllocatedSize());
			}
			return OutputCollections;
		}
#endif
		for (TSharedRef<const FManagedArrayCollection> ClothCollection : InClothCollections)
		{
			UE_LOG(LogChaosClothAsset, Display, TEXT("TrimOnCook [OFF] %s:[%d] [size:%d]"),
				*InAssetName, Index++, ClothCollection->GetAllocatedSize());
		}
		return InClothCollections;
	}
}

UChaosClothAsset::UChaosClothAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DataflowInstance.SetDataflowTerminal(TEXT("ClothAssetTerminal"));
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Setup a single LOD's Cloth Collection
	TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>();
	UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(ClothCollection);
	ClothFacade.DefineSchema();
	GetClothCollections().Emplace(MoveTemp(ClothCollection));
}

UChaosClothAsset::UChaosClothAsset(FVTableHelper& Helper)
	: Super(Helper)
{
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
UChaosClothAsset::~UChaosClothAsset() = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UChaosClothAsset::Serialize(FArchive& Ar)
{
	using namespace UE::Chaos::ClothAsset;

	LLM_SCOPE_BYNAME(TEXT("Physics/Cloth"));
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (bCooked && Ar.IsSaving())
	{
		TArray<TSharedRef<const FManagedArrayCollection>> OutputCollections = 
			Private::TrimOnCook(GetPathName(), GetClothCollections());
		Chaos::FChaosArchive ChaosArchive(Ar);
		Private::Serialize(ChaosArchive, OutputCollections);
	}
	else
	{
		Chaos::FChaosArchive ChaosArchive(Ar);
		Private::Serialize(ChaosArchive, GetClothCollections());
	}

#if WITH_EDITOR
	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::RecalculateClothAssetSerializedBounds)
	{
		CalculateBounds();
	}
#endif

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AddClothAssetBase)
	{
		Ar << GetRefSkeleton(); // Moved to Cloth Asset Base serialization
	}

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::ClothAssetSkinweightsValidation)
	{
		// Fix the skeleton mesh binding, which can cause crashes in the render code, or make the sim mesh disappear when missing
		for (TSharedRef<const FManagedArrayCollection>& ClothCollection : GetClothCollections())
		{
			const FCollectionClothConstFacade ClothConstFacade(ClothCollection);
			if (ClothConstFacade.IsValid())
			{
				const bool bHasValidSimSkinweights = Private::HasValidSkinweights(ClothConstFacade.GetSimBoneIndices(), ClothConstFacade.GetSimBoneWeights(), &GetRefSkeleton());
				const bool bHasValidRenderSkinweights = Private::HasValidSkinweights(ClothConstFacade.GetRenderBoneIndices(), ClothConstFacade.GetRenderBoneWeights(), &GetRefSkeleton());
				if (!bHasValidSimSkinweights || !bHasValidRenderSkinweights)
				{
					TSharedRef<FManagedArrayCollection> NewClothCollection = MakeShared<FManagedArrayCollection>(*ClothCollection);
					FClothGeometryTools::BindMeshToRootBone(NewClothCollection, !bHasValidSimSkinweights, !bHasValidRenderSkinweights);
					ClothCollection = MoveTemp(NewClothCollection);

					UE_CLOG(!bHasValidSimSkinweights, LogChaosClothAsset, Warning, TEXT("%s had invalid simulation mesh skin weights. This asset must be resaved."), *GetFullName());
					UE_CLOG(!bHasValidRenderSkinweights, LogChaosClothAsset, Warning, TEXT("%s had invalid render mesh skin weights. This asset must be resaved."), *GetFullName());
				}
			}
		}
	}

	if (bCooked && !IsTemplate() && !Ar.IsCountingMemory())  // Counting of these resources are done in GetResourceSizeEx, so skip these when counting memory
	{
		{
			LLM_SCOPE_BYNAME(TEXT("Physics/ClothRendering"));
			if (Ar.IsLoading())
			{
				SetResourceForRendering(MakeUnique<FSkeletalMeshRenderData>());
			}
			GetResourceForRendering()->Serialize(Ar, this);
		}

		if (!ClothSimulationModel.IsValid())
		{
			ClothSimulationModel = MakeShared<FChaosClothSimulationModel>();
		}
		UScriptStruct* const Struct = FChaosClothSimulationModel::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, (uint8*)ClothSimulationModel.Get(), Struct, nullptr);
	}
}

void UChaosClothAsset::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (DataflowAsset_DEPRECATED != nullptr)
	{
		SetDataflow(DataflowAsset_DEPRECATED);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		DataflowInstance.SetDataflowTerminal(FName(DataflowTerminal_DEPRECATED));
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		DataflowAsset_DEPRECATED = nullptr;
		DataflowTerminal_DEPRECATED.Empty();
	}
#endif  // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void UChaosClothAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UChaosClothAsset, PhysicsAsset))
	{
		ReregisterComponents();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UChaosClothAsset::ExecuteBuildInternal(FSkinnedAssetBuildContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::ExecuteBuildInternal);

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	// rebuild render data from imported model
	CacheDerivedData(&Context);

	// Build the material channel data used by the texture streamer
	UpdateUVChannelData(true);
}

void UChaosClothAsset::BeginBuildInternal(FSkinnedAssetBuildContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::BeginBuildInternal);

	SetInternalFlags(EInternalObjectFlags::Async);

	// Unregister all instances of this component
	Context.RecreateRenderStateContext = MakeUnique<FSkinnedMeshComponentRecreateRenderStateContext>(this, false);

	// Release the render data resources.
	ReleaseResources();

	// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
	// allocated, and potentially accessing the UChaosClothAsset.
	ReleaseResourcesFence.Wait();

	// Lock all properties that should not be modified/accessed during async post-load
	AcquireAsyncProperty();
}

void UChaosClothAsset::FinishBuildInternal(FSkinnedAssetBuildContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::FinishBuildInternal);

	ClearInternalFlags(EInternalObjectFlags::Async);

	ReleaseAsyncProperty();
}
#endif // #if WITH_EDITOR

void UChaosClothAsset::BeginPostLoadInternal(FSkinnedAssetPostLoadContext& Context)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::BeginPostLoadInternal);

	using namespace UE::Chaos::ClothAsset;

	checkf(IsInGameThread(), TEXT("Cannot execute function UChaosClothAsset::BeginPostLoadInternal asynchronously. Asset: %s"), *GetFullName());
	SetInternalFlags(EInternalObjectFlags::Async);

	// Lock all properties that should not be modified/accessed during async post-load
	AcquireAsyncProperty();

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	// Make sure that there is at least one valid collection
	if (GetClothCollections().IsEmpty())
	{
		UE_LOG(LogChaosClothAsset, Warning, TEXT("Invalid Cloth Collection (no LODs) found while loading Cloth Asset %s."), *GetFullName());
		TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>();
		FCollectionClothFacade ClothFacade(ClothCollection);
		ClothFacade.DefineSchema();
		GetClothCollections().Emplace(MoveTemp(ClothCollection));
	}

	// Check that all LODs have the cloth schema
	const int32 NumLods = GetClothCollections().Num();
	check(NumLods >= 1);  // The default LOD 0 should be present now if it ever was missing
	for (int32 LODIndex = 0; LODIndex < NumLods; ++LODIndex)
	{
		const TSharedRef<const FManagedArrayCollection>& ClothCollection = GetClothCollections()[LODIndex];

		const FCollectionClothConstFacade ClothConstFacade(ClothCollection);
		if (!ClothConstFacade.IsValid())
		{
			UE_LOG(LogChaosClothAsset, Warning, TEXT("Invalid Cloth Collection found at LOD %i while loading Cloth Asset %s."), LODIndex, *GetFullName());
			TSharedRef<FManagedArrayCollection> NewClothCollection = MakeShared<FManagedArrayCollection>();
			FCollectionClothFacade NewClothFacade = FCollectionClothFacade(NewClothCollection);
			NewClothFacade.DefineSchema();
			GetClothCollections()[LODIndex] = MoveTemp(NewClothCollection);
		}
	}

	// We're done touching the ClothCollections, so can unlock for read
	ReleaseAsyncProperty((uint64)EClothAssetAsyncProperties::ClothCollection, ESkinnedAssetAsyncPropertyLockType::WriteOnly);

	// Build the cloth simulation model
	BuildClothSimulationModel();  // TODO: Cache ClothSimulationModel in the DDC

	// Convert PerPlatForm data to PerQuality if perQuality data have not been serialized.
	// Also test default value, since PerPlatformData can have Default !=0 and no PerPlatform data overrides.
	const bool bConvertMinLODData = (MinQualityLevelLOD.PerQuality.Num() == 0 && MinQualityLevelLOD.Default == 0) && (MinLod.PerPlatform.Num() != 0 || MinLod.Default != 0);
	if (IsMinLodQualityLevelEnable() && bConvertMinLODData)
	{
		constexpr bool bRequireAllPlatformsKnownTrue = true;
		MinQualityLevelLOD.ConvertQualityLevelDataUsingCVar(MinLod.PerPlatform, MinLod.Default, bRequireAllPlatformsKnownTrue);
	}
#endif // #if WITH_EDITOR
}

void UChaosClothAsset::ExecutePostLoadInternal(FSkinnedAssetPostLoadContext& Context)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::ExecutePostLoadInternal);

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	if (!GetOutermost()->bIsCookedForEditor)
	{
		if (GetResourceForRendering() == nullptr)
		{
			CacheDerivedData(&Context);
			Context.bHasCachedDerivedData = true;
		}
	}
#endif // WITH_EDITOR
}

void UChaosClothAsset::FinishPostLoadInternal(FSkinnedAssetPostLoadContext& Context)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::FinishPostLoadInternal);

	checkf(IsInGameThread(), TEXT("Cannot execute function UChaosClothAsset::FinishPostLoadInternal asynchronously. Asset: %s"), *this->GetFullName());
	ClearInternalFlags(EInternalObjectFlags::Async);

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);
#endif

	if (FApp::CanEverRender())
	{
		InitResources();
	}
	else
	{
		// Update any missing data when cooking.
		UpdateUVChannelData(false);
	}

	CalculateInvRefMatrices();

#if WITH_EDITOR
	ReleaseAsyncProperty();
#endif
}

#if WITH_EDITOR
void UChaosClothAsset::CalculateBounds()
{
	using namespace UE::Chaos::ClothAsset;

	FBox BoundingBox(ForceInit);

	for (const TSharedRef<const FManagedArrayCollection>& ClothCollection : const_cast<const UChaosClothAsset*>(this)->GetClothCollections())
	{
		const FCollectionClothConstFacade Cloth(ClothCollection);
		const TConstArrayView<FVector3f> RenderPositionArray = Cloth.GetRenderPosition();

		for (const FVector3f& RenderPosition : RenderPositionArray)
		{
			BoundingBox += (FVector)RenderPosition;
		}
	}

	Bounds = FBoxSphereBounds(BoundingBox);
}
#endif

void UChaosClothAsset::Build(
	const TArray<TSharedRef<const FManagedArrayCollection>>& InClothCollections,
	TArray<FChaosClothAssetLodTransitionDataCache>* InOutTransitionCache,
	FText* ErrorText,
	FText* VerboseText)
{
	using namespace UE::Chaos::ClothAsset;

	// Reset the asset's collection
	TArray<TSharedRef<const FManagedArrayCollection>>& OutClothCollections = GetClothCollections();
	OutClothCollections.Reset(InClothCollections.Num());

	// Reset the asset's material list
	TArray<FSkeletalMaterial>& OutMaterials = GetMaterials();
	OutMaterials.Reset();

	// Iterate through the LODs
	FString PhysicsAssetPathName;
	for (int32 LodIndex = 0; LodIndex < InClothCollections.Num(); ++LodIndex)
	{
		// New LOD
		TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>();
		UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(ClothCollection);
		ClothFacade.DefineSchema();

		const FCollectionClothConstFacade InClothFacade(InClothCollections[LodIndex]);
		if (!InClothFacade.HasValidRenderData())  // The cloth collection must at least have a render mesh
		{
			if (ErrorText && ErrorText->IsEmpty())
			{
				*ErrorText = LOCTEXT("BuildErrorText", "Invalid LOD.");
				if (VerboseText)
				{
					*VerboseText = FText::Format(LOCTEXT("BuildVerboseTextFirstError", "LOD {0} has no valid data."), LodIndex);
				}
			}
			else if (ErrorText && VerboseText)
			{
				*VerboseText = FText::Format(LOCTEXT("BuildVerboseTextThereafter", "{0}\nLOD {1} has no valid data."), *VerboseText, LodIndex);
			}
			// else no error reporting

			OutClothCollections.Emplace(MoveTemp(ClothCollection));
			continue;
		}

		// Copy input LOD to current output LOD
		ClothFacade.Initialize(InClothFacade);

		// Add this LOD's materials to the asset
		const int32 NumLodMaterials = ClothFacade.GetNumRenderPatterns();

		OutMaterials.Reserve(OutMaterials.Num() + NumLodMaterials);

		const TConstArrayView<FString> LodRenderMaterialPathName = ClothFacade.GetRenderMaterialPathName();
		for (int32 LodMaterialIndex = 0; LodMaterialIndex < NumLodMaterials; ++LodMaterialIndex)
		{
			const FString& RenderMaterialPathName = LodRenderMaterialPathName[LodMaterialIndex];

			if (UMaterialInterface* const Material = LoadObject<UMaterialInterface>(this, *RenderMaterialPathName, nullptr, LOAD_None, nullptr))
			{
				OutMaterials.Emplace(Material, true, false, Material->GetFName());
			}
			else
			{
				OutMaterials.Emplace();  // Must keep one material slots per render pattern, do not change this behavior without updating the fix up code in GetOutfitClothCollectionsNode.cpp
			}
		}

		// Set properties
		constexpr bool bUpdateExistingProperties = false;
		Chaos::Softs::FCollectionPropertyMutableFacade PropertyFacade(ClothCollection);
		PropertyFacade.Append(InClothCollections[LodIndex].ToSharedPtr(), bUpdateExistingProperties);

		// Set selections
		FCollectionClothSelectionFacade Selection(ClothCollection);
		FCollectionClothSelectionConstFacade InSelection(InClothCollections[LodIndex]);
		if (InSelection.IsValid())
		{
			Selection.DefineSchema();
			const TArray<FName> InSelectionNames = InSelection.GetNames();
			for (const FName& InSelectionName : InSelectionNames)
			{
				const FName SelectionGroup = InSelection.GetSelectionGroup(InSelectionName);
				if (SelectionGroup == ClothCollectionGroup::SimVertices3D ||
					SelectionGroup == ClothCollectionGroup::SimFaces)
				{
					Selection.FindOrAddSelectionSet(InSelectionName, SelectionGroup) = InSelection.GetSelectionSet(InSelectionName);
				}
			}
		}

		// Set springs
		Chaos::Softs::FEmbeddedSpringFacade EmbeddedSpringFacade(ClothCollection.Get(), ClothCollectionGroup::SimVertices3D);
		const Chaos::Softs::FEmbeddedSpringFacade InEmbeddedSpringFacade(InClothCollections[LodIndex].Get(), ClothCollectionGroup::SimVertices3D);
		if (InEmbeddedSpringFacade.IsValid())
		{
			EmbeddedSpringFacade.DefineSchema();
			constexpr int32 VertexOffset = 0;
			EmbeddedSpringFacade.Append(InEmbeddedSpringFacade, VertexOffset);
		}

		// Set physics asset and skeleton source only with LOD 0 at the moment
		if (LodIndex == 0)
		{
			using namespace ::Chaos::Softs;
			PhysicsAssetPathName = InClothFacade.GetPhysicsAssetPathName();
			const FString SkeletalMeshPathName = InClothFacade.GetSkeletalMeshPathName(); 
			USkeletalMesh* const SkeletalMesh = SkeletalMeshPathName.IsEmpty() ? nullptr :
				LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPathName, nullptr, LOAD_None, nullptr);

			// Set reference skeleton
			SetSkeleton(SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr); // For completion only, this is not being used and might mismatch the skeletal mesh's reference skeleton
			Super::SetReferenceSkeleton(SkeletalMesh ? &SkeletalMesh->GetRefSkeleton() : nullptr);
		}

		// Fix the skeleton mesh binding if needed, which can cause crashes in the render code, or make the sim mesh disappear
		const bool bHasValidSimSkinweights = Private::HasValidSkinweights(InClothFacade.GetSimBoneIndices(), InClothFacade.GetSimBoneWeights(), &GetRefSkeleton());
		const bool bHasValidRenderSkinweights = Private::HasValidSkinweights(InClothFacade.GetRenderBoneIndices(), InClothFacade.GetRenderBoneWeights(), &GetRefSkeleton());
		if (!ensureAlwaysMsgf(bHasValidSimSkinweights && bHasValidRenderSkinweights, TEXT("A Dataflow node, likely an import node, has generated missing or invalid skin weights in this collection LOD. This must be fixed ASAP!")))
		{
			FClothGeometryTools::BindMeshToRootBone(ClothCollection, !bHasValidSimSkinweights, !bHasValidRenderSkinweights);
		}

		OutClothCollections.Emplace(MoveTemp(ClothCollection));
	}

	// Make sure that whatever happens there is always at least one empty LOD to avoid crashing the render data
	if (OutClothCollections.Num() < 1)
	{
		TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>();
		FCollectionClothFacade ClothFacade(ClothCollection);
		ClothFacade.DefineSchema();
		OutClothCollections.Emplace(MoveTemp(ClothCollection));
	}

	// Set physics asset (note: the cloth asset's physics asset is only replaced if a collection path name is found valid)
	PhysicsAsset = !PhysicsAssetPathName.IsEmpty() ?
		LoadObject<UPhysicsAsset>(this, *PhysicsAssetPathName, nullptr, LOAD_None, nullptr) :
		nullptr;

	SetHasVertexColors(true);

	// Rebuild the asset static data
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Build(InOutTransitionCache);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UChaosClothAsset::Build(TArray<FChaosClothAssetLodTransitionDataCache>* InOutTransitionCache)
{
	using namespace UE::Chaos::ClothAsset;

#if WITH_EDITOR
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	FSkinnedAssetBuildContext Context;
	BeginBuildInternal(Context);
#else
	ReleaseResources();
#endif

	// Set a new Guid to invalidate the DDC
	AssetGuid = FGuid::NewGuid();

	// Rebuild matrices
	CalculateInvRefMatrices();

	// Add LODs to the render data
	const int32 NumLods = FMath::Max(GetClothCollections().Num(), 1);  // The render data will always look for at least one default LOD 0

	// Rebuild LOD Infos
	LODInfo.Reset(NumLods);
	LODInfo.AddDefaulted(NumLods);  // TODO: Expose some properties to fill up the LOD infos

	// Build simulation model
	BuildClothSimulationModel(InOutTransitionCache);

#if WITH_EDITOR
	// Update bounds
	CalculateBounds();

	// Load/save render data from/to DDC
	ExecuteBuildInternal(Context);
#endif

	if (FApp::CanEverRender())
	{
		InitResources();
	}

#if WITH_EDITOR
	FinishBuildInternal(Context);
#endif

	// Re-register any components using this asset to restart the simulation with the updated asset
	ReregisterComponents();
}

#if WITH_EDITOR
void UChaosClothAsset::PrepareMeshModel()
{
	WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties::ImportedModel);
	WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties::ClothCollection);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const int32 NumLods = ClothCollections.Num();

	// Reset current LOD models
	MeshModel = MakeShareable(new FSkeletalMeshModel());
	MeshModel->LODModels.Reset(NumLods);

	// Rebuild each LOD models
	for (int32 LodIndex = 0; LodIndex < NumLods; ++LodIndex)
	{
		MeshModel->LODModels.Add(new FSkeletalMeshLODModel());
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif  // #if WITH_EDITOR

void UChaosClothAsset::BuildClothSimulationModel(TArray<FChaosClothAssetLodTransitionDataCache>* InOutTransitionCache)
{
	ClothSimulationModel = MakeShared<FChaosClothSimulationModel>(const_cast<const UChaosClothAsset*>(this)->GetClothCollections(), 
		GetRefSkeleton(), InOutTransitionCache);
}

FString UChaosClothAsset::GetAsyncPropertyName(uint64 Property) const
{
	return StaticEnum<EClothAssetAsyncProperties>()->GetNameByValue(Property).ToString();
}

const TArray<TSharedRef<const FManagedArrayCollection>>& UChaosClothAsset::GetCollections(int32 /*ModelIndex*/) const
{
	WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties::ClothCollection);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return ClothCollections;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#if WITH_EDITOR
void UChaosClothAsset::CacheDerivedData(FSkinnedAssetCompilationContext* Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::CacheDerivedData);
	check(Context);

	// Cache derived data for the running platform.
	ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(RunningPlatform);

	// Create the render data
	SetResourceForRendering(MakeUnique<FSkeletalMeshRenderData>());

	// Prepare the LOD Model array with the number of LODs for when the cache DDC function regenerates the models
	PrepareMeshModel();

	// Load render data from DDC, or generate it and save to DDC
	GetResourceForRendering()->Cache(RunningPlatform, this, Context);
}

void UChaosClothAsset::BuildLODModel(FSkeletalMeshRenderData& RenderData, const ITargetPlatform* TargetPlatform, int32 LODIndex)
{
	check(GetImportedModel() && GetImportedModel()->LODModels.IsValidIndex(LODIndex));
	FBuilder::BuildLod(GetImportedModel()->LODModels[LODIndex], *this, LODIndex, TargetPlatform);
}

FString UChaosClothAsset::BuildDerivedDataKey(const ITargetPlatform* TargetPlatform)
{
	FString KeySuffix(TEXT(""));
	KeySuffix += AssetGuid.ToString();

	FString TmpPartialKeySuffix;
	//Synchronize the user data that are part of the key
	GetImportedModel()->SyncronizeLODUserSectionsData();

	// Model GUID is not generated so exclude GetImportedModel()->GetIdString() from DDC key.

	// Add the hashed string generated from the model data 
	TmpPartialKeySuffix = GetImportedModel()->GetLODModelIdString();
	KeySuffix += TmpPartialKeySuffix;

	//Add the max gpu bone per section
	const int32 MaxGPUSkinBones = FGPUBaseSkinVertexFactory::GetMaxGPUSkinBones(TargetPlatform);
	KeySuffix += FString::FromInt(MaxGPUSkinBones);
	// Add unlimited bone influences mode
	IMeshBuilderModule::GetForPlatform(TargetPlatform).AppendToDDCKey(KeySuffix, true);
	const bool bUnlimitedBoneInfluences = FGPUBaseSkinVertexFactory::GetUnlimitedBoneInfluences(TargetPlatform);
	KeySuffix += bUnlimitedBoneInfluences ? "1" : "0";

	// Include the global default bone influences limit in case any LODs don't set an explicit limit (highly likely)
	KeySuffix += FString::FromInt(GetDefault<URendererSettings>()->DefaultBoneInfluenceLimit.GetValueForPlatform(*TargetPlatform->IniPlatformName()));

	// Add LODInfoArray
	TmpPartialKeySuffix = TEXT("");
	TArray<FSkeletalMeshLODInfo>& LODInfos = GetLODInfoArray();
	for (int32 LODIndex = 0; LODIndex < GetLODNum(); ++LODIndex)
	{
		check(LODInfos.IsValidIndex(LODIndex));
		FSkeletalMeshLODInfo& LOD = LODInfos[LODIndex];
		LOD.BuildGUID = LOD.ComputeDeriveDataCacheKey(nullptr);	// TODO: FSkeletalMeshLODGroupSettings
		TmpPartialKeySuffix += LOD.BuildGUID.ToString(EGuidFormats::Digits);
	}
	KeySuffix += TmpPartialKeySuffix;

	//KeySuffix += GetHasVertexColors() ? "1" : "0";
	//KeySuffix += GetVertexColorGuid().ToString(EGuidFormats::Digits);

	//if (GetEnableLODStreaming(TargetPlatform))
	//{
	//	const int32 MaxNumStreamedLODs = GetMaxNumStreamedLODs(TargetPlatform);
	//	const int32 MaxNumOptionalLODs = GetMaxNumOptionalLODs(TargetPlatform);
	//	KeySuffix += *FString::Printf(TEXT("1%08x%08x"), MaxNumStreamedLODs, MaxNumOptionalLODs);
	//}
	//else
	//{
	//	KeySuffix += TEXT("0zzzzzzzzzzzzzzzz");
	//}

	//if (TargetPlatform->GetPlatformInfo().PlatformGroupName == TEXT("Desktop")
	//	&& GStripSkeletalMeshLodsDuringCooking != 0
	//	&& GSkeletalMeshKeepMobileMinLODSettingOnDesktop != 0)
	//{
	//	KeySuffix += TEXT("_MinMLOD");
	//}

	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("CHAOSCLOTH"),
		CHAOS_CLOTH_ASSET_DERIVED_DATA_VERSION,
		*KeySuffix
	);
}

bool UChaosClothAsset::IsInitialBuildDone() const
{
	//We are consider built if we have a valid lod model
	return GetImportedModel() != nullptr &&
		GetImportedModel()->LODModels.Num() > 0 &&
		GetImportedModel()->LODModels[0].Sections.Num() > 0;
}
#endif // WITH_EDITOR

bool UChaosClothAsset::HasValidClothSimulationModels() const
{
	return ClothSimulationModel && ClothSimulationModel->GetNumLods();
}

void UChaosClothAsset::SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset)
{
	using namespace UE::Chaos::ClothAsset;

	PhysicsAsset = InPhysicsAsset;
}

void UChaosClothAsset::SetReferenceSkeleton(const FReferenceSkeleton* ReferenceSkeleton, bool bRebuildModels, bool bRebindMeshes)
{
	using namespace UE::Chaos::ClothAsset;

	// Update the reference skeleton
	Super::SetReferenceSkeleton(ReferenceSkeleton);

	// Rebuild the models
	if (bRebuildModels)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Build();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

void UChaosClothAsset::UpdateSkeletonFromCollection(bool /*bRebuildModels*/)
{
	using namespace UE::Chaos::ClothAsset;

	check(GetClothCollections().Num());
	FCollectionClothConstFacade ClothFacade(GetClothCollections()[0]);
	check(ClothFacade.IsValid());

	const FString& SkeletalMeshPathName = ClothFacade.GetSkeletalMeshPathName();
	USkeletalMesh* const SkeletalMesh = SkeletalMeshPathName.IsEmpty() ? nullptr :
		LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPathName, nullptr, LOAD_None, nullptr);

	SetSkeleton(SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr); // For completion only, this is not being used and might mismatch the skeletal mesh's reference skeleton
	Super::SetReferenceSkeleton(SkeletalMesh ? &SkeletalMesh->GetRefSkeleton() : nullptr);
}

void UChaosClothAsset::CopySimMeshToRenderMesh(UMaterialInterface* Material)
{
	using namespace UE::Chaos::ClothAsset;
	check(GetClothCollections().Num());

	// Add a default material if none is specified
	const FString RenderMaterialPathName = Material ?
		Material->GetPathName() :
		FString(TEXT("/Engine/EditorMaterials/Cloth/CameraLitDoubleSided.CameraLitDoubleSided"));

	bool bAnyLodHasRenderMesh = false;
	for (TSharedRef<const FManagedArrayCollection>& ClothCollection : GetClothCollections())
	{
		TSharedRef<FManagedArrayCollection> NewClothCollection = MakeShared<FManagedArrayCollection>(*ClothCollection);
		constexpr bool bSingleRenderPattern = true;
		FClothGeometryTools::CopySimMeshToRenderMesh(NewClothCollection, RenderMaterialPathName, bSingleRenderPattern);
		bAnyLodHasRenderMesh = bAnyLodHasRenderMesh || FClothGeometryTools::HasRenderMesh(NewClothCollection);
		ClothCollection = MoveTemp(NewClothCollection);
	}

	// Set new material
	Materials.Reset(1);
	if (bAnyLodHasRenderMesh)
	{
		if (UMaterialInterface* const LoadedMaterial = LoadObject<UMaterialInterface>(nullptr, *RenderMaterialPathName, nullptr, LOAD_None, nullptr))
		{
			Materials.Emplace(LoadedMaterial, true, false, LoadedMaterial->GetFName());
		}
	}
}

#undef LOCTEXT_NAMESPACE
