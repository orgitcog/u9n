// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "DataflowCollectionAddScalarVertexPropertyNode.h"
#include "DataflowPrimitiveNode.h"
#include "DataflowSkeletalMeshNodes.h"
#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Dataflow/DataflowCollectionAttributeKeyNodes.h"

#include "DataflowCollectionEditSkinWeightsNode.generated.h"

/** Dataflow skin weights data */
USTRUCT()
struct DATAFLOWNODES_API FDataflowSkinWeightData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<float> BoneWeights;

	UPROPERTY()
	TArray<int32> BoneIndices;
};

/** Edit skin weights vertex properties. */
USTRUCT(Meta = (Experimental,DataflowCollection))
struct DATAFLOWNODES_API FDataflowCollectionEditSkinWeightsNode : public FDataflowPrimitiveNode
{
	GENERATED_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowCollectionEditSkinWeightsNode, "EditSkinWeights", "Collection", "Edit skin weights and save it to collection")

public:

	FDataflowCollectionEditSkinWeightsNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** The name to be set as a weight map attribute. */
	UPROPERTY(EditAnywhere, Category = "Vertex Attributes", meta = (DisplayName = "Bone Indices"))
	FString BoneIndicesName;

	/** The name to be set as a weight map attribute. */
	UPROPERTY(EditAnywhere, Category = "Vertex Attributes", meta = (DisplayName = "Bone Weights"))
	FString BoneWeightsName;

	/** Target group in which the attributes are stored */
	UPROPERTY(EditAnywhere, Category = "Vertex Attributes", meta = (DisplayName = "Vertex Group"))
	FScalarVertexPropertyGroup VertexGroup;

	/** Bone indices key to be used in other nodes if necessary */
	UPROPERTY(meta = (DisplayName = "Bone Indices", DataflowInput, DataflowOutput, DataflowPassthrough = "BoneIndicesKey"))
	FCollectionAttributeKey BoneIndicesKey;

	/** Bone weights key to be used in other nodes if necessary */
	UPROPERTY(meta = (DisplayName = "Bone Weights", DataflowInput, DataflowOutput, DataflowPassthrough = "BoneWeightsKey"))
	FCollectionAttributeKey BoneWeightsKey;

	/** Skeletal mesh to extract the skeleton from for the skinning */
	UPROPERTY(EditAnywhere, Category = "Skeleton Binding")
	TObjectPtr<USkeleton> ObjectSkeleton = nullptr;

	/** Boolean to use a compressed format (FVector4f, FIntVector) to store the skin weights */
	UPROPERTY(EditAnywhere, Category = "Skeleton Binding")
	bool bCompressSkinWeights = false;

	/** List of skin weights */
	UPROPERTY()
	TArray<FDataflowSkinWeightData> SkinWeights;
	
	/** Delegate to transfer the bone selection to the tool*/
	FDataflowBoneSelectionChangedNotifyDelegate OnBoneSelectionChanged;

	/** Report the vertex weights back onto the property ones */
	void ReportVertexWeights(const TArray<TArray<int32>>& SetupIndices, const TArray<TArray<float>>& SetupWeights, const TArray<TArray<int32>>& FinalIndices, const TArray<TArray<float>>& FinalWeights);

	/** Extract the vertex weights back from the property ones */
	void ExtractVertexWeights(const TArray<TArray<int32>>& SetupIndices, const TArray<TArray<float>>& SetupWeights, TArrayView<TArray<int32>> FinalIndices, TArrayView<TArray<float>> FinalWeights) const;

	/** Fill the weight attribute values from the collection */
	static bool FillAttributeWeights(const FManagedArrayCollection& SelectedCollection,
		const FCollectionAttributeKey& IndicesAttributeKey, const FCollectionAttributeKey& WeightsAttributeKey, TArray<TArray<int32>>& AttributeIndices, TArray<TArray<float>>& AttributeWeights);

	/** Get the weight attribute values and add them if not there*/
	static bool GetAttributeWeights(FManagedArrayCollection& SelectedCollection,
			const FCollectionAttributeKey& InBoneIndicesKey, const FCollectionAttributeKey& InBoneWeightsKey,
			TArray<TArray<int32>>& AttributeIndices, TArray<TArray<float>>& AttributeWeights, const bool bCanCompressSkinWeights);

	/** Set back the attribute values into the collection */
	static bool SetAttributeWeights(FManagedArrayCollection& SelectedCollection,
			const FCollectionAttributeKey& InBoneIndicesKey, const FCollectionAttributeKey& InBoneWeightsKey,
			const TArray<TArray<int32>>& AttributeIndices, const TArray<TArray<float>>& AttributeWeights);

	/** Get the weights attribute key to retrieve/set the weight values*/
	FCollectionAttributeKey GetBoneIndicesKey(UE::Dataflow::FContext& Context) const;

	/** Get the weights attribute key to retrieve/set the weight values*/
    FCollectionAttributeKey GetBoneWeightsKey(UE::Dataflow::FContext& Context) const;

	/** Validate the skeletal mesh construction */
	void ValidateSkeletalMeshes() {bValidSkeletalMeshes = true;}

	/** Return the collection offset given a skeletal mesh*/
	int32 GetSkeletalMeshOffset(const TObjectPtr<USkeletalMesh>& SkeletalMesh) const;
	
private:

	//~ Begin FDataflowPrimitiveNode interface
	virtual TArray<UE::Dataflow::FRenderingParameter> GetRenderParametersImpl() const override;
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	virtual void AddPrimitiveComponents(const TSharedPtr<const FManagedArrayCollection> RenderCollection,
		TObjectPtr<UObject> NodeOwner, TObjectPtr<AActor> RootActor, TArray<TObjectPtr<UPrimitiveComponent>>& PrimitiveComponents) override;
	virtual void OnInvalidate() override;
#if WITH_EDITOR
	virtual bool CanDebugDraw() const override { return true; }
	virtual bool CanDebugDrawViewMode(const FName& ViewModeName) const override;
	virtual void DebugDraw(UE::Dataflow::FContext& Context,
		IDataflowDebugDrawInterface& DataflowRenderingInterface,
		const FDebugDrawParameters& DebugDrawParameters) const override;
#endif
	//~ End FDataflowPrimitiveNode interface

	/** Transient skeletal mesh built from dataflow render collection */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMesh>> SkeletalMeshes;

	/** Valid skeletal mesh boolean to trigger the construction */
	bool bValidSkeletalMeshes = false;
};
