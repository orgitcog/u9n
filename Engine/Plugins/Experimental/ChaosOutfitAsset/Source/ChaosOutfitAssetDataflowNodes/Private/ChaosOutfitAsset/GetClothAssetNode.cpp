// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosOutfitAsset/GetClothAssetNode.h"
#include "ChaosClothAsset/ClothAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GetClothAssetNode)

#define LOCTEXT_NAMESPACE "ChaosGetClothAssetNode"

FChaosGetClothAssetNode::FChaosGetClothAssetNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&ClothAsset);
}

void FChaosGetClothAssetNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TObjectPtr<const UChaosClothAsset>>(&ClothAsset))
	{
		SetValue(Context, ClothAsset, &ClothAsset);
	}
}

#undef LOCTEXT_NAMESPACE
