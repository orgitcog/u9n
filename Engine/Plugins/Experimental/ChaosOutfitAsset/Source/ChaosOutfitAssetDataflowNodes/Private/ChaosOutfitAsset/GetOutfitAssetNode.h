// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GetOutfitAssetNode.generated.h"

class UChaosOutfitAsset;

/**
 * Get an outfit asset object into the graph.
 */
USTRUCT(Meta = (Experimental, DataflowOutfit))
struct FChaosGetOutfitAssetNode final : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosGetOutfitAssetNode, "GetOutfitAsset", "Outfit", "Get Outfit Asset")

public:
	FChaosGetOutfitAssetNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	//~ End FDataflowNode interface

	/** The Outfit Asset to import into a collection. */
	UPROPERTY(EditAnywhere, Category = "GetOutfitAsset", Meta = (DataflowOutput))
	TObjectPtr<const UChaosOutfitAsset> OutfitAsset;
};
