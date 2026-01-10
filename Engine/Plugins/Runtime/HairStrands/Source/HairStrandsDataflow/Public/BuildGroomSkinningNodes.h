// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GroomAsset.h"
#include "ChaosLog.h"
#include "GroomCollectionFacades.h"
#include "Dataflow/DataflowConnectionTypes.h"
#include "Dataflow/DataflowCore.h"

#include "BuildGroomSkinningNodes.generated.h"

/** Build the guides skinning by transferring the indices weights from a skelmesh */
USTRUCT(meta = (Experimental, DataflowGroom))
struct FTransferSkinWeightsGroomNode : public FDataflowNode
{
	GENERATED_BODY()
	
	DATAFLOW_NODE_DEFINE_INTERNAL(FTransferSkinWeightsGroomNode, "TransferSkinWeights", "Groom", "")

public:
	
	FTransferSkinWeightsGroomNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
		RegisterOutputConnection(&BoneIndicesKey);
		RegisterOutputConnection(&BoneWeightsKey);
	}

	//~ Begin FDataflowNode interface
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	virtual TArray<UE::Dataflow::FRenderingParameter> GetRenderParametersImpl() const override;
	//~ End FDataflowNode interface

	/** Managed array collection to be used to store datas */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough  = "Collection"))
	FManagedArrayCollection Collection;

	/** SkeletalMesh used to transfer the skinning weights. Will be stored onto the groom asset */
	UPROPERTY(EditAnywhere, Category="Skeletal Mesh")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	/** LOD used to transfer the weights */
	UPROPERTY(EditAnywhere, Category="Skeletal Mesh", meta = (DisplayName = "LOD Index"))
	int32 LODIndex = 0;
	
	/** Group index on which the dats will be transfered. -1 will transfer on all the groups */
	UPROPERTY(EditAnywhere, Category="Groom Groups", meta = (DisplayName = "Group Index"))
	int32 GroupIndex = INDEX_NONE;

	/** The relative transform between the skeletal mesh and the groom asset. */
	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh")
	FTransform RelativeTransform;

	/** Type of curves to use to fill the groom collection (guides/strands) */
	UPROPERTY(EditAnywhere, Category = "Groom Groups", meta = (DisplayName = "Curves Type"))
	EGroomCollectionType CurvesType = EGroomCollectionType::Guides;
	
	/** Bone indices key to be used in other nodes if necessary */
	UPROPERTY(meta = (DisplayName = "Bone Indices", DataflowOutput))
	FCollectionAttributeKey BoneIndicesKey;

	/** Bone weights key to be used in other nodes if necessary */
	UPROPERTY(meta = (DisplayName = "Bone Weights", DataflowOutput))
	FCollectionAttributeKey BoneWeightsKey;

private :
	/** Get the bone indices key */
	FCollectionAttributeKey GetBoneIndicesKey() const;

	/** Get the bone weights key */
	FCollectionAttributeKey GetBoneWeightsKey() const;
};


