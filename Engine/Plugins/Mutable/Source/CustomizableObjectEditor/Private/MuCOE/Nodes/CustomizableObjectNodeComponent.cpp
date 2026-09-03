// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeComponent.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMacroInstance.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"


void UCustomizableObjectNodeComponent::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	OutputPin = CustomCreatePin(EGPD_Output, UEdGraphSchema_CustomizableObject::PC_Component, FName(TEXT("Component")));
	ComponentNamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, FName("Name"));
}


FLinearColor UCustomizableObjectNodeComponent::GetNodeTitleColor() const
{
	return UEdGraphSchema_CustomizableObject::GetPinTypeColor(UEdGraphSchema_CustomizableObject::PC_Component);
}


bool UCustomizableObjectNodeComponent::IsAffectedByLOD() const
{ 
	return false; 
}


void UCustomizableObjectNodeComponent::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);

	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::EnableMutableMacrosNewVersion)
	{
		if (!ComponentNamePin.Get())
		{
			ComponentNamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, FName("Name"));
		}
	}
}


bool UCustomizableObjectNodeComponent::GetCanRenameNode() const
{
	const UEdGraphPin* NamePin = GetComponentNamePin();

	if (NamePin && NamePin->LinkedTo.Num())
	{
		return false;
	}

	return true;
}


void UCustomizableObjectNodeComponent::OnRenameNode(const FString& NewName)
{
	if (!NewName.IsEmpty())
	{
		SetComponentName(FName(*NewName));
	}
}


void UCustomizableObjectNodeComponent::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Pin == GetComponentNamePin())
	{
		GetGraph()->NotifyGraphChanged();
	}
}


FName UCustomizableObjectNodeComponent::GetComponentName(TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext) const
{
	const UEdGraphPin* NamePin = GetComponentNamePin();

	if (NamePin)
	{
		if (const UEdGraphPin* LinkedPin = FollowInputPin(*NamePin))
		{
			const UEdGraphPin* StringPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*LinkedPin, MacroContext);

			if (const UCustomizableObjectNodeStaticString* StringNode = StringPin ? Cast<UCustomizableObjectNodeStaticString>(StringPin->GetOwningNode()) : nullptr)
			{
				return FName(StringNode->Value);
			}
		}
	}

	return ComponentName;
}


void UCustomizableObjectNodeComponent::SetComponentName(const FName& InComponentName)
{
	ComponentName = InComponentName;
}


UEdGraphPin* UCustomizableObjectNodeComponent::GetComponentNamePin() const
{
	return ComponentNamePin.Get();
}
