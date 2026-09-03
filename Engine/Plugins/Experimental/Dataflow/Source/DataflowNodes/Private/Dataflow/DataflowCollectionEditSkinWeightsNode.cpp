// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowCollectionEditSkinWeightsNode.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowDebugDrawObject.h"
#include "Dataflow/DataflowSkeletalMeshNodes.h"
#include "Dataflow/CollectionRenderingPatternUtility.h"

#include "DynamicMeshToMeshDescription.h"
#include "MeshConversionOptions.h"
#include "SkeletalMeshAttributes.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryCollection/Facades/CollectionRenderingFacade.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"

#if WITH_EDITOR
#include "StaticToSkeletalMeshConverter.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowCollectionEditSkinWeightsNode)

#define LOCTEXT_NAMESPACE "DataflowCollectionEditSkinWeights"

namespace UE::Dataflow::Private
{

static bool CreateSkeletalMesh(USkeletalMesh* SkeletalMesh, const TArray<UE::Geometry::FDynamicMesh3>& DynamicMeshes, const FReferenceSkeleton& ReferenceSkeleton)
{
#if WITH_EDITOR
	TArray<const FMeshDescription*> MeshDescriptions;

#if WITH_EDITORONLY_DATA
	SkeletalMesh->PreEditChange(nullptr);
	SkeletalMesh->GetImportedModel()->LODModels.Empty();
#endif
	SkeletalMesh->ResetLODInfo();

	TArray<FMeshDescription> LocalDescriptions;
	for (const UE::Geometry::FDynamicMesh3& DynamicMesh : DynamicMeshes)
	{
		// Create a mesh description
		FMeshDescription& MeshDescription = LocalDescriptions.AddDefaulted_GetRef();
	
		// Add skeletal mesh attributes to the mesh description
		FSkeletalMeshAttributes Attributes(MeshDescription);
		Attributes.Register();

		// Convert dynamic mesh to the mesh description
		FConversionToMeshDescriptionOptions ConverterOptions;
		FDynamicMeshToMeshDescription Converter(ConverterOptions);
		Converter.Convert(&DynamicMesh, MeshDescription, false);

		// Add the created description to the list
		MeshDescriptions.Add(&MeshDescription);
	}

	TArray<FSkeletalMaterial> Materials;
	TConstArrayView<FSkeletalMaterial> MaterialView;

	// ensure there is at least one material
	if (MaterialView.IsEmpty())
	{
		Materials.Add( UMaterial::GetDefaultMaterial(MD_Surface));
		MaterialView = Materials;
	}

	static constexpr bool bRecomputeTangents = false;
	static constexpr bool bRecomputeNormals = false;
	static constexpr bool bCacheOptimize = false;

	if (!FStaticToSkeletalMeshConverter::InitializeSkeletalMeshFromMeshDescriptions(
		SkeletalMesh, MeshDescriptions, MaterialView, ReferenceSkeleton, bRecomputeNormals, bRecomputeTangents, bCacheOptimize))
	{
		return false;
	}
	return true;
#else
	return false;
#endif
}

static bool BuildSkeletalMeshes(TArray<TObjectPtr<USkeletalMesh>>& SkeletalMeshes, 
	const TSharedPtr<const FManagedArrayCollection>& RenderCollection, const TObjectPtr<USkeleton>& ObjectSkeleton)
{
	if(RenderCollection.IsValid() && ObjectSkeleton)
	{
		GeometryCollection::Facades::FRenderingFacade Facade(*RenderCollection);
		if(Facade.IsValid())
		{
			bool bValidSkeletalMeshes = true;
			const int32 NumGeometry = Facade.NumGeometry();

			if(NumGeometry == SkeletalMeshes.Num())
			{
				for (int32 MeshIndex = 0; MeshIndex < NumGeometry; ++MeshIndex)
				{
					UE::Geometry::FDynamicMesh3 DynamicMesh;
					UE::Dataflow::Conversion::RenderingFacadeToDynamicMesh(Facade, MeshIndex, DynamicMesh);

					if(!CreateSkeletalMesh(SkeletalMeshes[MeshIndex], {DynamicMesh}, ObjectSkeleton->GetReferenceSkeleton()))
					{
						bValidSkeletalMeshes = false;
					}
				}
			}
			return bValidSkeletalMeshes;
		}
	}
	return false;
}
	
template<typename ScalarType, typename VectorType, int32 NumComponents>
bool SetAttributeValues(FManagedArrayCollection& SelectedCollection,
	const FCollectionAttributeKey& AttributeKey, const TArray<TArray<ScalarType>>& AttributeValues, const ScalarType& DefaultValue, const bool bRenormalizeValues)
{
	if (!AttributeValues.IsEmpty() && !AttributeKey.Attribute.IsEmpty() && !AttributeKey.Group.IsEmpty())
	{
		const FName AttributeName(AttributeKey.Attribute);
		const FName AttributeGroup(AttributeKey.Group);
		
		if(TManagedArray<TArray<ScalarType>>* AttributeArray = SelectedCollection.FindAttributeTyped<TArray<ScalarType>>(AttributeName, AttributeGroup))
		{
			if(AttributeArray->Num() == AttributeValues.Num())
			{
				for(int32 VertexIndex = 0, NumVertices = AttributeArray->Num(); VertexIndex < NumVertices; ++VertexIndex)
				{
					AttributeArray->GetData()[VertexIndex] = AttributeValues[VertexIndex];
				}
			}
			return true;
		}
		else if(TManagedArray<VectorType>* AttributeVector = SelectedCollection.FindAttributeTyped<VectorType>(AttributeName, AttributeGroup))
		{
			if(AttributeVector->Num() == AttributeValues.Num())
			{
				for(int32 VertexIndex = 0, NumVertices = AttributeVector->Num(); VertexIndex < NumVertices; ++VertexIndex)
				{
					VectorType& ElementVector = AttributeVector->GetData()[VertexIndex];
					const int32 NumValidComponents = FMath::Min(NumComponents, AttributeValues[VertexIndex].Num());
					
					float TotalValue = 0.0f;
					for(int32 ComponentIndex = 0; ComponentIndex < NumValidComponents; ++ComponentIndex)
					{
						ElementVector[ComponentIndex] = AttributeValues[VertexIndex][ComponentIndex];
						TotalValue += ElementVector[ComponentIndex];
					}

					for(int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
					{
						ElementVector[ComponentIndex] = (ComponentIndex >= NumValidComponents) ? DefaultValue : (bRenormalizeValues && TotalValue != 0.0f)
							?  ElementVector[ComponentIndex] / TotalValue : ElementVector[ComponentIndex];
					}
				}
			}
			return true;
		}
	}
	return false;
}

template<typename ScalarType, typename VectorType, int32 NumComponents>
bool FillAttributeValues(const FManagedArrayCollection& SelectedCollection,
	const FCollectionAttributeKey& AttributeKey, TArray<TArray<ScalarType>>& AttributeValues)
{
	if (!AttributeKey.Attribute.IsEmpty() && !AttributeKey.Group.IsEmpty())
	{
		const FName AttributeName(AttributeKey.Attribute);
		const FName AttributeGroup(AttributeKey.Group);

		if(const TManagedArray<TArray<ScalarType>>* AttributeArray = SelectedCollection.FindAttributeTyped<TArray<ScalarType>>(AttributeName, AttributeGroup))
		{
			AttributeValues = AttributeArray->GetConstArray();
			return true;
		}
		else if(const TManagedArray<VectorType>* AttributeVector = SelectedCollection.FindAttributeTyped<VectorType>(AttributeName, AttributeGroup))
		{
			AttributeValues.SetNum(AttributeVector->Num());
			for(int32 VertexIndex = 0, NumVertices = AttributeVector->Num(); VertexIndex < NumVertices; ++VertexIndex)
			{
				const VectorType& ElementVector = AttributeVector->GetConstArray()[VertexIndex];
				AttributeValues[VertexIndex].SetNum(NumComponents);
				
				for(int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
				{
					AttributeValues[VertexIndex][ComponentIndex] = ElementVector[ComponentIndex];
				}
			}
			return true;
		}
	}
	return false;
}

template<typename ScalarType, typename VectorType, int32 NumComponents>
bool GetAttributeValues(FManagedArrayCollection& SelectedCollection,
	const FCollectionAttributeKey& AttributeKey, TArray<TArray<ScalarType>>& AttributeValues, const bool bVectorValues)
{
	if (!AttributeKey.Attribute.IsEmpty() && !AttributeKey.Group.IsEmpty())
	{
		const FName AttributeName(AttributeKey.Attribute);
		const FName AttributeGroup(AttributeKey.Group);

		if(SelectedCollection.FindAttributeTyped<TArray<ScalarType>>(AttributeName, AttributeGroup) == nullptr &&
			SelectedCollection.FindAttributeTyped<VectorType>(AttributeName, AttributeGroup) == nullptr)
		{
			if(bVectorValues)
			{
				SelectedCollection.AddAttribute<VectorType>(AttributeName, AttributeGroup);
			}
			else
			{
				SelectedCollection.AddAttribute<TArray<ScalarType>>(AttributeName, AttributeGroup);
			}
		}
	}
	return FillAttributeValues<ScalarType,VectorType, NumComponents>(SelectedCollection, AttributeKey, AttributeValues);
}

void CorrectSkinWeights(TArray<TArray<int32>>& BoneIndices, TArray<TArray<float>>& BoneWeights)
{
	check(BoneIndices.Num() == BoneWeights.Num());

	TArray<int32> ValidIndices;
	TArray<float> ValidWeights;
	
	for(int32 VertexIndex = 0, NumVertices = BoneWeights.Num(); VertexIndex < NumVertices; ++VertexIndex)
	{
		if(BoneWeights[VertexIndex].Num() == BoneIndices[VertexIndex].Num())
		{
			ValidIndices.Reset(BoneIndices.Num());
			ValidWeights.Reset(BoneWeights.Num());
			
			for(int32 WeightIndex = 0, NumWeights = BoneWeights[VertexIndex].Num(); WeightIndex < NumWeights; ++WeightIndex)
			{
				if(BoneIndices[VertexIndex][WeightIndex] != INDEX_NONE)
				{
					ValidIndices.Add(BoneIndices[VertexIndex][WeightIndex]);
					ValidWeights.Add(BoneWeights[VertexIndex][WeightIndex]);
				}
			}
			BoneIndices[VertexIndex] = ValidIndices;
			BoneWeights[VertexIndex] = ValidWeights;
		}
	}
}

}

//
// FDataflowCollectionEditSkinWeightsNode
//

FDataflowCollectionEditSkinWeightsNode::FDataflowCollectionEditSkinWeightsNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowPrimitiveNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&BoneIndicesKey);
	RegisterInputConnection(&BoneWeightsKey);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&BoneIndicesKey, &BoneIndicesKey);
	RegisterOutputConnection(&BoneWeightsKey, &BoneWeightsKey);
}

TArray<UE::Dataflow::FRenderingParameter> FDataflowCollectionEditSkinWeightsNode::GetRenderParametersImpl() const
{
	return FDataflowAddScalarVertexPropertyCallbackRegistry::Get().GetRenderingParameters(VertexGroup.Name);
}

void FDataflowCollectionEditSkinWeightsNode::AddPrimitiveComponents(const TSharedPtr<const FManagedArrayCollection> RenderCollection, TObjectPtr<UObject> NodeOwner, 
	TObjectPtr<AActor> RootActor, TArray<TObjectPtr<UPrimitiveComponent>>& PrimitiveComponents)  
{
	if(RootActor)
	{
		GeometryCollection::Facades::FRenderingFacade RenderingFacade(*RenderCollection);
		const int32 NumGeometry = RenderingFacade.IsValid() ? RenderingFacade.NumGeometry() : 0;
		
		const bool bNeedsConstruction = (SkeletalMeshes.Num() != NumGeometry) || !bValidSkeletalMeshes;
		
		if(SkeletalMeshes.Num() != NumGeometry)
		{
			SkeletalMeshes.Reset();
			for(int32 GeometryIndex = 0; GeometryIndex < NumGeometry; ++GeometryIndex)
			{
				FString SkeletalMeshName = FString("SK_DataflowSkeletalMesh_") + FString::FromInt(GeometryIndex);
				SkeletalMeshName = MakeUniqueObjectName(NodeOwner, USkeletalMesh::StaticClass(), *SkeletalMeshName, EUniqueObjectNameOptions::GloballyUnique).ToString();
				SkeletalMeshes.Add(NewObject<USkeletalMesh>(NodeOwner, FName(*SkeletalMeshName), RF_Transient));
			}
		}
		if(bNeedsConstruction)
		{
			bValidSkeletalMeshes = UE::Dataflow::Private::BuildSkeletalMeshes(SkeletalMeshes, RenderCollection, ObjectSkeleton);
			if(!bValidSkeletalMeshes)
			{
				SkeletalMeshes.Reset();
			}
		}
		if(!SkeletalMeshes.IsEmpty())
		{
			for(int32 GeometryIndex = 0; GeometryIndex < NumGeometry; ++GeometryIndex)
			{
				FName SkeletalMeshName(FString("Dataflow_SkeletalMesh") + FString::FromInt(GeometryIndex));
				USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(RootActor, SkeletalMeshName);
				SkeletalMeshComponent->SetSkeletalMesh(SkeletalMeshes[GeometryIndex]);
				PrimitiveComponents.Add(SkeletalMeshComponent);
			}
		}
	}
}

void FDataflowCollectionEditSkinWeightsNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Dataflow;
	
	// Get the pin value if plugged
	FCollectionAttributeKey BoneIndicesKeyValue = GetBoneIndicesKey(Context);
	FCollectionAttributeKey BoneWeightsKeyValue = GetBoneWeightsKey(Context);

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		if (!BoneIndicesKeyValue.Attribute.IsEmpty() && !BoneWeightsKeyValue.Attribute.IsEmpty() )
		{
			TArray<TArray<float>> SetupWeights, FinalWeights;
			TArray<TArray<int32>> SetupIndices, FinalIndices;
			
			if(GetAttributeWeights(InCollection, BoneIndicesKeyValue, BoneWeightsKeyValue, SetupIndices, SetupWeights, bCompressSkinWeights))
			{
				FinalIndices.SetNum(SetupIndices.Num());
				FinalWeights.SetNum(SetupWeights.Num());
				
				ExtractVertexWeights(SetupIndices, SetupWeights, TArrayView<TArray<int32>>(FinalIndices.GetData(), FinalIndices.Num()),
				TArrayView<TArray<float>>(FinalWeights.GetData(), FinalWeights.Num()));

				SetAttributeWeights(InCollection, BoneIndicesKeyValue, BoneWeightsKeyValue, FinalIndices, FinalWeights);
			}
		}

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
	else if (Out->IsA<FCollectionAttributeKey>(&BoneIndicesKey))
	{
		SetValue(Context, MoveTemp(BoneIndicesKeyValue), &BoneIndicesKey);
	}
	else if (Out->IsA<FCollectionAttributeKey>(&BoneWeightsKey))
	{
		SetValue(Context, MoveTemp(BoneWeightsKeyValue), &BoneWeightsKey);
	}
}

void FDataflowCollectionEditSkinWeightsNode::ReportVertexWeights(const TArray<TArray<int32>>& SetupIndices, const TArray<TArray<float>>& SetupWeights, const TArray<TArray<int32>>& FinalIndices, const TArray<TArray<float>>& FinalWeights)
{
	check(SetupWeights.Num() == FinalWeights.Num());
	check(SetupWeights.Num() == SetupIndices.Num());
	check(FinalWeights.Num() == FinalIndices.Num());
	
	SkinWeights.SetNumZeroed(FinalWeights.Num());
	
	for (int32 VertexIndex = 0, NumVertices = FinalWeights.Num(); VertexIndex < NumVertices; ++VertexIndex)
	{
		SkinWeights[VertexIndex].BoneWeights = FinalWeights[VertexIndex];
		SkinWeights[VertexIndex].BoneIndices = FinalIndices[VertexIndex];
	}
}

void FDataflowCollectionEditSkinWeightsNode::ExtractVertexWeights(const TArray<TArray<int32>>& SetupIndices, const TArray<TArray<float>>& SetupWeights,
	TArrayView<TArray<int32>> FinalIndices, TArrayView<TArray<float>> FinalWeights) const
{
	check(SetupWeights.Num() == FinalWeights.Num());
	check(SetupWeights.Num() == SetupIndices.Num());
	check(FinalWeights.Num() == FinalIndices.Num());

	if(SkinWeights.Num() == FinalWeights.Num())
	{
		for (int32 VertexIndex = 0, NumVertices = FinalWeights.Num(); VertexIndex < NumVertices; ++VertexIndex)
		{
			if(!SkinWeights[VertexIndex].BoneWeights.IsEmpty() && !SkinWeights[VertexIndex].BoneIndices.IsEmpty())
			{
				FinalWeights[VertexIndex] = SkinWeights[VertexIndex].BoneWeights;
				FinalIndices[VertexIndex] = SkinWeights[VertexIndex].BoneIndices;
			}
			else
			{
				FinalWeights[VertexIndex] = SetupWeights[VertexIndex];
				FinalIndices[VertexIndex] = SetupIndices[VertexIndex];
			}
		}
	}
	else
	{
		for (int32 VertexIndex = 0, NumVertices = FinalWeights.Num(); VertexIndex < NumVertices; ++VertexIndex)
		{
			FinalWeights[VertexIndex] = SetupWeights[VertexIndex];
			FinalIndices[VertexIndex] = SetupIndices[VertexIndex];
		}
	}
}

bool FDataflowCollectionEditSkinWeightsNode::SetAttributeWeights(FManagedArrayCollection& SelectedCollection,
	const FCollectionAttributeKey& InBoneIndicesKey, const FCollectionAttributeKey& InBoneWeightsKey,
	const TArray<TArray<int32>>& AttributeIndices, const TArray<TArray<float>>& AttributeWeights)
{
	return UE::Dataflow::Private::SetAttributeValues<int32, FIntVector4, 4>(SelectedCollection, InBoneIndicesKey, AttributeIndices,  INDEX_NONE, false) && 
		   UE::Dataflow::Private::SetAttributeValues<float, FVector4f, 4>(SelectedCollection, InBoneWeightsKey, AttributeWeights, 0.0f, true);
}

bool FDataflowCollectionEditSkinWeightsNode::GetAttributeWeights(FManagedArrayCollection& SelectedCollection,
	const FCollectionAttributeKey& InBoneIndicesKey, const FCollectionAttributeKey& InBoneWeightsKey,
	TArray<TArray<int32>>& AttributeIndices, TArray<TArray<float>>& AttributeWeights, const bool bCanCompressSkinWeights)
{
	const bool bValidAttributes =
		UE::Dataflow::Private::GetAttributeValues<int32, FIntVector4, 4>(SelectedCollection, InBoneIndicesKey, AttributeIndices, bCanCompressSkinWeights) && 
		UE::Dataflow::Private::GetAttributeValues<float, FVector4f, 4>(SelectedCollection, InBoneWeightsKey, AttributeWeights, bCanCompressSkinWeights);

	UE::Dataflow::Private::CorrectSkinWeights(AttributeIndices, AttributeWeights);

	return bValidAttributes;
}

bool FDataflowCollectionEditSkinWeightsNode::FillAttributeWeights(const FManagedArrayCollection& SelectedCollection,
	const FCollectionAttributeKey& InBoneIndicesKey, const FCollectionAttributeKey& InBoneWeightsKey,
	TArray<TArray<int32>>& AttributeIndices, TArray<TArray<float>>& AttributeWeights)
{
	const bool bValidAttributes =
		UE::Dataflow::Private::FillAttributeValues<int32, FIntVector4, 4>(SelectedCollection, InBoneIndicesKey, AttributeIndices) &&
		UE::Dataflow::Private::FillAttributeValues<float, FVector4f, 4>(SelectedCollection, InBoneWeightsKey, AttributeWeights);

	UE::Dataflow::Private::CorrectSkinWeights(AttributeIndices, AttributeWeights);

	return bValidAttributes;
}

FCollectionAttributeKey FDataflowCollectionEditSkinWeightsNode::GetBoneIndicesKey(UE::Dataflow::FContext& Context) const
{
	// Get the pin value if plugged
	FCollectionAttributeKey Key = GetValue(Context, &BoneIndicesKey, BoneIndicesKey);

	// If nothing set used the local value
	if(Key.Attribute.IsEmpty() && Key.Group.IsEmpty())
	{
		Key.Group = VertexGroup.Name.ToString();
		Key.Attribute = BoneIndicesName;
	}
	return Key;
}

FCollectionAttributeKey FDataflowCollectionEditSkinWeightsNode::GetBoneWeightsKey(UE::Dataflow::FContext& Context) const
{
	// Get the pin value if plugged
	FCollectionAttributeKey Key = GetValue(Context, &BoneWeightsKey, BoneWeightsKey);

	// If nothing set used the local value
	if(Key.Attribute.IsEmpty() && Key.Group.IsEmpty())
	{
		Key.Group = VertexGroup.Name.ToString();
		Key.Attribute = BoneWeightsName;
	}
	return Key;
}

void FDataflowCollectionEditSkinWeightsNode::OnInvalidate()
{
	bValidSkeletalMeshes = false;
}

#if WITH_EDITOR

void FDataflowCollectionEditSkinWeightsNode::DebugDraw(UE::Dataflow::FContext& Context,
		IDataflowDebugDrawInterface& DataflowRenderingInterface,
		const FDebugDrawParameters& DebugDrawParameters) const
{
	if(ObjectSkeleton)
	{
		TRefCountPtr<FDataflowDebugDrawSkeletonObject> SkeletonObject = MakeDebugDrawObject<FDataflowDebugDrawSkeletonObject>(
			DataflowRenderingInterface.ModifyDataflowElements(), ObjectSkeleton->GetReferenceSkeleton());
	
		DataflowRenderingInterface.DrawObject(TRefCountPtr<IDataflowDebugDrawObject>(SkeletonObject));

		SkeletonObject->OnBoneSelectionChanged.AddLambda([this](const TArray<FName>& BoneNames)
		{
			OnBoneSelectionChanged.Broadcast(BoneNames);
		});
	}
}

bool FDataflowCollectionEditSkinWeightsNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return true;
}

#endif

int32 FDataflowCollectionEditSkinWeightsNode::GetSkeletalMeshOffset(const TObjectPtr<USkeletalMesh>& SkeletalMesh) const
{
#if WITH_EDITORONLY_DATA
	int32 SkeletalMeshOffset = 0;
	for(int32 SkeletalMeshIndex = 0, NumMeshes = SkeletalMeshes.Num(); SkeletalMeshIndex < NumMeshes; ++SkeletalMeshIndex)
	{
		if (SkeletalMeshes[SkeletalMeshIndex] == SkeletalMesh)
		{
			return SkeletalMeshOffset;
		}
		else if (FMeshDescription* MeshDescription = SkeletalMeshes[SkeletalMeshIndex]->GetMeshDescription(0))
		{
			SkeletalMeshOffset += MeshDescription->Vertices().Num();
		}
	}
#endif
	return INDEX_NONE;
}

#undef LOCTEXT_NAMESPACE
