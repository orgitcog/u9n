// Copyright Epic Games, Inc. All Rights Reserved.


#include "RigMapperDefinitionEditorGraphNode.h"

#include "SRigMapperDefinitionGraphEditorNode.h"
#include "RigMapperDefinition.h"
#include "RigMapperDefinitionEditorGraph.h"
#include "EdGraph/EdGraphPin.h"

#define LOCTEXT_NAMESPACE "RigMapperDefinitionEditorGraphNode"

TSharedPtr<SGraphNode> URigMapperDefinitionEditorGraphNode::NodeFactory::CreateNode(UEdGraphNode* Node) const
{
	if (URigMapperDefinitionEditorGraphNode* ThisNode = Cast<URigMapperDefinitionEditorGraphNode>(Node))
	{
		if (URigMapperDefinitionEditorGraph* Graph = Cast<URigMapperDefinitionEditorGraph>(Node->GetGraph()))
		{
			Graph->RequestRefreshLayout(true);
		}

		TSharedRef<SGraphNode> GraphNode = SNew(SRigMapperDefinitionGraphEditorNode, ThisNode);
		GraphNode->SlatePrepass();
		ThisNode->SetDimensions(GraphNode->GetDesiredSize());
		return GraphNode;
	}
	return nullptr;
}

FLinearColor URigMapperDefinitionEditorGraphNode::GetNodeBodyTintColor() const
{
	return GetNodeTitleColor();
}

FLinearColor URigMapperDefinitionEditorGraphNode::GetNodeTitleColor() const
{
	if (NodeType == ERigMapperNodeType::Input)
	{
		return FLinearColor(0.466, 0.969, 0.878);
	}
	if (NodeType == ERigMapperNodeType::Output)
	{
		return FLinearColor(0.466, 0.969, 0.525);
	}
	if (NodeType == ERigMapperNodeType::NullOutput)
	{
		return FLinearColor(0.65, 0.969, 0.466);
	}
	if (NodeType == ERigMapperNodeType::Multiply)
	{
		return FLinearColor(0.969, 0.966, 0.466);
	}
	if (NodeType == ERigMapperNodeType::SDK)
	{
		return FLinearColor(0.969, 0.717, 0.466);
	}
	if (NodeType == ERigMapperNodeType::WeightedSum)
	{
		return FLinearColor(0.65, 0.466, 0.969);
	}
	return FLinearColor(0.969, 0.5, 0.466);
}

void URigMapperDefinitionEditorGraphNode::SetupNode(URigMapperDefinition* InDefinition, const FString& InNodeName, ERigMapperNodeType InNodeType)
{
	NodeName = InNodeName;
	Definition = InDefinition;
	
	NodeTitle = FText::FromString(NodeName);
	SetNodeType(InNodeType);
}

void URigMapperDefinitionEditorGraphNode::SetNodeType(ERigMapperNodeType InNodeType)
{
	NodeType = InNodeType;

	if (Definition.IsValid() && (NodeType == ERigMapperNodeType::WeightedSum || NodeType == ERigMapperNodeType::SDK))
	{
		ERigMapperFeatureType FeatureType;
		if (FRigMapperFeature* Feature = Definition.Get()->Features.Find(NodeName, FeatureType))
		{
			if (FeatureType == ERigMapperFeatureType::SDK)
			{
				TArray<FString> InKeys;
				TArray<FString> OutKeys;
				
				const TArray<FRigMapperSdkKey>& Keys = static_cast<FRigMapperSdkFeature*>(Feature)->Keys;

				InKeys.Reserve(Keys.Num());
				OutKeys.Reserve(Keys.Num());
				for (const FRigMapperSdkKey& Key : Keys)
				{
					InKeys.Add(FString::Printf(TEXT("%.*f"), 3, Key.In));
					OutKeys.Add(FString::Printf(TEXT("%.*f"), 3, Key.Out));
				}
				const FString In = FString::Join(InKeys, TEXT(", "));
				const FString Out = FString::Join(OutKeys, TEXT(", "));
				NodeSubtitle = FText::FromString(FString::Printf(TEXT("[%s] > [%s]"), *In, *Out));
			}
			if (FeatureType == ERigMapperFeatureType::WeightedSum)
			{
				const TMap<FString, double>& Inputs = static_cast<FRigMapperWsFeature*>(Feature)->Inputs;
				const FRigMapperFeatureRange& Range = static_cast<FRigMapperWsFeature*>(Feature)->Range;
				const FText MinText = LOCTEXT("RangeMinimum", "min");
				const FText MaxText = LOCTEXT("RangeMaximum", "max");

				for (UEdGraphPin* InPin : InputPins)
				{
					for (const UEdGraphPin* OutPin : InPin->LinkedTo)
					{
						const FString LinkedNodeName = OutPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
						if (Inputs.Contains(LinkedNodeName))
						{
							InPin->PinFriendlyName = FText::Format(INVTEXT("{0}"), Inputs[LinkedNodeName]);
							FString RangeStr;
							if (Range.bHasLowerBound)
							{
								RangeStr.Appendf(TEXT("%s: %.*f"), *MinText.ToString(), 3, Range.LowerBound);
							}
							if (Range.bHasLowerBound && Range.bHasUpperBound)
							{
								RangeStr.Append(TEXT("\n"));
							}
							if (Range.bHasUpperBound)
							{
								RangeStr.Appendf(TEXT("%s: %.*f"), *MaxText.ToString(), 3, Range.UpperBound);
							}
							NodeSubtitle = FText::FromString(RangeStr);
						}	
					}
				}
			}
		}
		
	}

}

UEdGraphPin* URigMapperDefinitionEditorGraphNode::CreateInputPin()
{
	return InputPins.Add_GetRef(CreatePin(EEdGraphPinDirection::EGPD_Input, NAME_None, NAME_None, NAME_None));
}

UEdGraphPin* URigMapperDefinitionEditorGraphNode::CreateOutputPin()
{
	return OutputPins.Add_GetRef(CreatePin(EEdGraphPinDirection::EGPD_Output, NAME_None, NAME_None, NAME_None));
}

void URigMapperDefinitionEditorGraphNode::GetRect(FVector2D& TopLeft, FVector2D& BottomRight) const
{
	TopLeft.X = NodePosX;
	TopLeft.Y = NodePosY;
	BottomRight.X = NodePosX + GetDimensions().X + GetMargin().X;
	BottomRight.Y = NodePosY + GetDimensions().Y + GetMargin().Y;
}

#undef LOCTEXT_NAMESPACE