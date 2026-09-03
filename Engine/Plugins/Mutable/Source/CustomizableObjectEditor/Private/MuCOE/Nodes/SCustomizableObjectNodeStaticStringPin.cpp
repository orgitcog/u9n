// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/SCustomizableObjectNodeStaticStringPin.h"

#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"
#include "MuCOE/Nodes/CustomizableObjectNodeComponentMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeComponentMeshAddTo.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectChild.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuCOE/Nodes/CustomizableObjectNodeParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTunnel.h"
#include "MuCOE/Nodes/CustomizableObjectNodeVariation.h"
#include "Widgets/Input/SEditableTextBox.h"


void SCustomizableObjectNodeStaticStringPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SCustomizableObjectNodePin::Construct(SCustomizableObjectNodePin::FArguments(), InGraphPinObj);

	// Hide pin Icon and Label
	if (InGraphPinObj->bNotConnectable)
	{
		SetShowLabel(false);
		PinImage->SetVisibility(EVisibility::Collapsed);
	}
}


TSharedRef<SWidget>	SCustomizableObjectNodeStaticStringPin::GetDefaultValueWidget()
{
	return SNew(SEditableTextBox)
		.Text(this, &SCustomizableObjectNodeStaticStringPin::GetNodeStringValue)
		.OnTextCommitted(this, &SCustomizableObjectNodeStaticStringPin::OnTextCommited)
		.Visibility(this, &SCustomizableObjectNodeStaticStringPin::GetWidgetVisibility);
}


FText SCustomizableObjectNodeStaticStringPin::GetNodeStringValue() const
{
	const UEdGraphNode* Node = GraphPinObj->GetOwningNode();

	if (const UCustomizableObjectNodeStaticString* StringNode = Cast<UCustomizableObjectNodeStaticString>(Node))
	{
		return FText::FromString(StringNode->Value);
	}
	else if (const UCustomizableObjectNodeComponent* ComponentNode = Cast<UCustomizableObjectNodeComponent>(Node))
	{
		return FText::FromName(ComponentNode->GetComponentName());
	}
	else if (const UCustomizableObjectNodeComponentMeshAddTo* AddComponentNode = Cast<UCustomizableObjectNodeComponentMeshAddTo>(Node))
	{
		return FText::FromName(AddComponentNode->GetParentComponentName());
	}
	else if (const UCustomizableObjectNodeMeshParameter* MeshParamNode = Cast<UCustomizableObjectNodeMeshParameter>(Node))
	{
		return FText::FromString(MeshParamNode->ParameterName);
	}
	else if (const UCustomizableObjectNodeObjectChild* ChildObjectNode = Cast<UCustomizableObjectNodeObjectChild>(Node))
	{
		return FText::FromString(ChildObjectNode->GetObjectName());
	}
	else if (const UCustomizableObjectNodeObjectGroup* GroupNode = Cast<UCustomizableObjectNodeObjectGroup>(Node))
	{
		return FText::FromString(GroupNode->GetGroupName());
	}
	else if (const UCustomizableObjectNodeParameter* ParameterNode = Cast< UCustomizableObjectNodeParameter>(Node))
	{
		return FText::FromString(ParameterNode->GetParameterName());
	}
	else if (const UCustomizableObjectNodeVariation* VariationNode = Cast<UCustomizableObjectNodeVariation>(Node))
	{
		for (int32 VariationIndex = 0; VariationIndex < VariationNode->GetNumVariations(); ++VariationIndex)
		{
			if (VariationNode->VariationTagPin(VariationIndex) == GraphPinObj)
			{
				return FText::FromString(VariationNode->GetVariationTag(VariationIndex));
			}
		}
	}

	return FText();
}


void SCustomizableObjectNodeStaticStringPin::OnTextCommited(const FText& InValue, ETextCommit::Type InCommitInfo)
{
	UEdGraphNode* Node = GraphPinObj->GetOwningNode();

	if (UCustomizableObjectNodeStaticString* StringNode = Cast<UCustomizableObjectNodeStaticString>(Node))
	{
		StringNode->Value = InValue.ToString();
	}
	else if (UCustomizableObjectNodeComponent* ComponentNode = Cast<UCustomizableObjectNodeComponent>(Node))
	{
		ComponentNode->SetComponentName(FName(*InValue.ToString()));
	}
	else if (UCustomizableObjectNodeComponentMeshAddTo* AddComponentNode = Cast<UCustomizableObjectNodeComponentMeshAddTo>(Node))
	{
		AddComponentNode->SetParentComponentName(FName(*InValue.ToString()));
	}
	else if (UCustomizableObjectNodeMeshParameter* MeshParamNode = Cast<UCustomizableObjectNodeMeshParameter>(Node))
	{
		MeshParamNode->ParameterName = InValue.ToString();
	}
	else if (UCustomizableObjectNodeObjectChild* ChildObjectNode = Cast<UCustomizableObjectNodeObjectChild>(Node))
	{
		ChildObjectNode->SetObjectName(InValue.ToString());
	}
	else if (UCustomizableObjectNodeObjectGroup* GroupNode = Cast<UCustomizableObjectNodeObjectGroup>(Node))
	{
		GroupNode->SetGroupName(InValue.ToString());
	}
	else if (UCustomizableObjectNodeParameter* ParameterNode = Cast<UCustomizableObjectNodeParameter>(Node))
	{
		ParameterNode->SetParameterName(InValue.ToString());
	}
	else if (UCustomizableObjectNodeVariation* VariationNode = Cast<UCustomizableObjectNodeVariation>(Node))
	{
		for (int32 VariationIndex = 0; VariationIndex < VariationNode->GetNumVariations(); ++VariationIndex)
		{
			if (VariationNode->VariationTagPin(VariationIndex) == GraphPinObj && VariationNode->VariationsData.IsValidIndex(VariationIndex))
			{
				VariationNode->VariationsData[VariationIndex].Tag = InValue.ToString();

				break;
			}
		}
	}
}


EVisibility SCustomizableObjectNodeStaticStringPin::GetWidgetVisibility() const
{
	return GraphPinObj->LinkedTo.Num() ? EVisibility::Collapsed : EVisibility::Visible;
}
