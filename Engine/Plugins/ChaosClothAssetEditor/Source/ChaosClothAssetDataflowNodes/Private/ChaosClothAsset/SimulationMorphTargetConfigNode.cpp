// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationMorphTargetConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationMorphTargetConfigNode)

FChaosClothAssetSimulationMorphTargetConfigNode::FChaosClothAssetSimulationMorphTargetConfigNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationMorphTargetConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyAndString(FName(TEXT("ActiveMorphTarget")), ActiveMorphTarget.Weight, ActiveMorphTarget.Name);
}
