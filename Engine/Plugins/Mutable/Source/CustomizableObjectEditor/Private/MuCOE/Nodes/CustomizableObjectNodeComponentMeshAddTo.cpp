// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeComponentMeshAddTo.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeComponentMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMacroInstance.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void UCustomizableObjectNodeComponentMeshAddTo::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	Super::AllocateDefaultPins(RemapPins);

	LODPins.Empty(NumLODs);
	for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		FString LODName = FString::Printf(TEXT("LOD %d"), LODIndex);

		UEdGraphPin* Pin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Material, FName(*LODName), true);
		LODPins.Add(Pin);
	}

	ParentComponentNamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, FName("Component Name"));
	OutputPin = CustomCreatePin(EGPD_Output, UEdGraphSchema_CustomizableObject::PC_Component, FName(TEXT("Component")));
}


void UCustomizableObjectNodeComponentMeshAddTo::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (!PropertyThatChanged)
	{
		return;
	}

	if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeComponentMeshAddTo, NumLODs))
	{
		ReconstructNode();
	}
}


bool UCustomizableObjectNodeComponentMeshAddTo::IsAffectedByLOD() const
{
	return false;
}


bool UCustomizableObjectNodeComponentMeshAddTo::IsSingleOutputNode() const
{
	// todo UE-225446 : By limiting the number of connections this node can have we avoid a check failure. However, this method should be
	// removed in the future and the inherent issue with 1:n output connections should be fixed in its place
	return true;
}


void UCustomizableObjectNodeComponentMeshAddTo::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);

	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::EnableMutableMacrosNewVersion)
	{
		if (!ParentComponentNamePin.Get())
		{
			ParentComponentNamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, FName("Component Name"));
		}
	}
}


FText UCustomizableObjectNodeComponentMeshAddTo::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UEdGraphPin* ParentNamePin = GetParentComponentNamePin();
	FText NodeTitle;
	FFormatNamedArguments Args;
	Args.Add(TEXT("ComponentName"), FText::FromName(ParentComponentName));

	if (TitleType == ENodeTitleType::ListView || (ParentNamePin && ParentNamePin->LinkedTo.Num()))
	{
		NodeTitle = LOCTEXT("ComponentMeshAdd", "Add To Mesh Component");
	}
	else if (TitleType == ENodeTitleType::EditableTitle)
	{
		NodeTitle = LOCTEXT("ComponentMeshAdd_Edit", "{ComponentName}");
	}
	else
	{
		NodeTitle = LOCTEXT("ComponentMeshAdd_Title", "{ComponentName}\nAdd To Mesh Component");
	}

	return FText::Format(NodeTitle, Args);
}


void UCustomizableObjectNodeComponentMeshAddTo::OnRenameNode(const FString& NewName)
{
	if (!NewName.IsEmpty())
	{
		ParentComponentName = FName(*NewName);
	}
}


FLinearColor UCustomizableObjectNodeComponentMeshAddTo::GetNodeTitleColor() const
{
	return UEdGraphSchema_CustomizableObject::GetPinTypeColor(UEdGraphSchema_CustomizableObject::PC_Component);
}


void UCustomizableObjectNodeComponentMeshAddTo::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Pin == GetParentComponentNamePin())
	{
		GetGraph()->NotifyGraphChanged();
	}
}


int32 UCustomizableObjectNodeComponentMeshAddTo::GetNumLODs()
{
	return NumLODs;
}


ECustomizableObjectAutomaticLODStrategy UCustomizableObjectNodeComponentMeshAddTo::GetAutoLODStrategy()
{
	return AutoLODStrategy;
}


const TArray<FEdGraphPinReference>& UCustomizableObjectNodeComponentMeshAddTo::GetLODPins() const
{
	return LODPins;
}


UEdGraphPin* UCustomizableObjectNodeComponentMeshAddTo::GetOutputPin() const
{
	return OutputPin.Get();
}


void UCustomizableObjectNodeComponentMeshAddTo::SetOutputPin(const UEdGraphPin* Pin)
{
	OutputPin = Pin;
}


const UCustomizableObjectNode* UCustomizableObjectNodeComponentMeshAddTo::GetOwningNode() const
{
	return this;
}


FName UCustomizableObjectNodeComponentMeshAddTo::GetParentComponentName(TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext) const
{
	const UEdGraphPin* ParentNamePin = GetParentComponentNamePin();
	if (ParentNamePin)
	{
		if (const UEdGraphPin* LinkedPin = FollowInputPin(*ParentNamePin))
		{
			const UEdGraphPin* StringPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*LinkedPin, MacroContext);

			if (const UCustomizableObjectNodeStaticString* StringNode = StringPin ? Cast<UCustomizableObjectNodeStaticString>(StringPin->GetOwningNode()) : nullptr)
			{
				return FName(StringNode->Value);
			}
		}
	}

	return ParentComponentName;
}


void UCustomizableObjectNodeComponentMeshAddTo::SetParentComponentName(const FName& InComponentName)
{
	ParentComponentName = InComponentName;
}


UEdGraphPin* UCustomizableObjectNodeComponentMeshAddTo::GetParentComponentNamePin() const
{
	return ParentComponentNamePin.Get();
}

#undef LOCTEXT_NAMESPACE

