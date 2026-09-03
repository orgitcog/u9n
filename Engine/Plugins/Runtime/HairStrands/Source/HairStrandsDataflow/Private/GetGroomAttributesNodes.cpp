// Copyright Epic Games, Inc. All Rights Reserved.

#include "GetGroomAttributesNodes.h"
#include "GroomCollectionFacades.h"
#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GetGroomAttributesNodes)

void FGetGroomAttributesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FCollectionAttributeKey>(&AttributeKey))
	{
		FCollectionAttributeKey Key;

		const FString VertexGroup = (CurvesType == EGroomCollectionType::Guides) ? UE::Groom::FGroomGuidesFacade::VerticesGroup.ToString() :
								UE::Groom::FGroomStrandsFacade::VerticesGroup.ToString();
		const FString CurveGroup = (CurvesType == EGroomCollectionType::Guides) ? UE::Groom::FGroomGuidesFacade::CurvesGroup.ToString() :
								UE::Groom::FGroomStrandsFacade::CurvesGroup.ToString();
		if(AttributeType == EGroomAttributeType::KinematicWeights)
		{ 
			Key.Group = VertexGroup;
			Key.Attribute = UE::Groom::FGroomGuidesFacade::PointKinematicWeightsAttribute.ToString();
		}
		else if (AttributeType == EGroomAttributeType::BoneIndices)
		{
			Key.Group = VertexGroup;
			Key.Attribute = UE::Groom::FGroomGuidesFacade::PointBoneIndicesAttribute.ToString();
		}
		else if (AttributeType == EGroomAttributeType::BoneWeights)
		{
			Key.Group = VertexGroup;
			Key.Attribute = UE::Groom::FGroomGuidesFacade::PointBoneWeightsAttribute.ToString();
		}
		else if (AttributeType == EGroomAttributeType::CurveLods)
		{
			Key.Group = CurveGroup;
			Key.Attribute = UE::Groom::FGroomGuidesFacade::CurveLodIndicesAttribute.ToString();
		}
		else if (AttributeType == EGroomAttributeType::CurveParents)
		{
			Key.Group = CurveGroup;
			Key.Attribute = UE::Groom::FGroomGuidesFacade::CurveParentIndicesAttribute.ToString();
		}
		SetValue(Context, MoveTemp(Key), &AttributeKey);
	}
}

