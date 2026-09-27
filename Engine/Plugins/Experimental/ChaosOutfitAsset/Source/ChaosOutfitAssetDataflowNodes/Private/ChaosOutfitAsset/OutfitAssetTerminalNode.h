// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowFunctionProperty.h"
#include "Dataflow/DataflowTerminalNode.h"
#include "OutfitAssetTerminalNode.generated.h"

class UChaosOutfit;

/**
 * Outfit terminal node to generate an outfit asset from several cloth assets.
 */
USTRUCT(Meta = (Experimental, DataflowOutfit, DataflowTerminal))
struct FChaosOutfitAssetTerminalNode final : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosOutfitAssetTerminalNode, "OutfitAssetTerminal", "Outfit", "Outfit Terminal")

public:
	FChaosOutfitAssetTerminalNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void SetAssetValue(TObjectPtr<UObject> Asset, UE::Dataflow::FContext& Context) const override;
	virtual void Evaluate(UE::Dataflow::FContext& Context) const override {}
	//~ End FDataflowNode interface

	/** Input outfit collection. */
	UPROPERTY(Meta = (DataflowInput))
	TObjectPtr<const UChaosOutfit> Outfit;

	/** Convert this outfit to a SkeletalMesh asset. */
	UPROPERTY(EditAnywhere, Category = "Outfit Asset Terminal", Meta = (ButtonImage = "Icons.Convert"))
	FDataflowFunctionProperty ConvertToSkeletalMesh;
};

