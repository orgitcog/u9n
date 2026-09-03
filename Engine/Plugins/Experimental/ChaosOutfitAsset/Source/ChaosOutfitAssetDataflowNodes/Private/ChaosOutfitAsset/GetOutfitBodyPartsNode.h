// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GetOutfitBodyPartsNode.generated.h"

class UChaosOutfit;
class USkeletalMesh;

USTRUCT()
struct FChaosOutfitBodySizeBodyParts
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY()
	TArray<TObjectPtr<const USkeletalMesh>> BodyParts;
};

/**
 * Extract the Body Part Skeletal Meshes from an Outfit.
 */
USTRUCT(Meta = (Experimental, DataflowOutfit))
struct FChaosGetOutfitBodyPartsNode final : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosGetOutfitBodyPartsNode, "GetOutfitBodyParts", "Outfit", "Outfit Body Parts Skeletal Mesh")

public:
	FChaosGetOutfitBodyPartsNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	//~ End FDataflowNode interface

	/** The source outfit. */
	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Outfit"))
	TObjectPtr<const UChaosOutfit> Outfit;

	/** The outfit body parts grouped by BodySize. */
	UPROPERTY(Meta = (DataflowOutput))
	TArray<FChaosOutfitBodySizeBodyParts> BodySizeParts;
};

/** Extract the array of BodyParts from a FChaosOutfitBodySizeBodyParts */
USTRUCT(Meta = (Experimental, DataflowOutfit))
struct FChaosExtractBodyPartsArrayFromBodySizePartsNode final : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosExtractBodyPartsArrayFromBodySizePartsNode, "ExtractBodyPartsArrayFromBodySizeParts", "Outfit", "Extract Outfit Body Parts Skeletal Mesh")

public:
	FChaosExtractBodyPartsArrayFromBodySizePartsNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	//~ End FDataflowNode interface

	/** The source outfit. */
	UPROPERTY(Meta = (DataflowInput))
	FChaosOutfitBodySizeBodyParts BodySizeParts;

	/** The outfit body parts grouped by BodySize. */
	UPROPERTY(Meta = (DataflowOutput))
	TArray<TObjectPtr<const USkeletalMesh>> BodyParts;
};
