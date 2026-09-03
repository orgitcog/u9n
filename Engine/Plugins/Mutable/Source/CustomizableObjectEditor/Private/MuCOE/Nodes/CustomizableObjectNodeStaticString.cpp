// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/Nodes/SCustomizableObjectNode.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectNodeStringConstant"


FText UCustomizableObjectNodeStaticString::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("StaticStringNodeTitle", "Static String");
}


FText UCustomizableObjectNodeStaticString::GetTooltipText() const
{
	return LOCTEXT("StaticStringNodeTooltip", "Static String Node");
}


FLinearColor UCustomizableObjectNodeStaticString::GetNodeTitleColor() const
{
	return UEdGraphSchema_CustomizableObject::GetPinTypeColor(UEdGraphSchema_CustomizableObject::PC_String);
}


bool UCustomizableObjectNodeStaticString::GetCanRenameNode() const
{
	return false;
}


void UCustomizableObjectNodeStaticString::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ValuePin = CustomCreatePin(EGPD_Output, Schema->PC_String, FName("Value"));
	UEdGraphPin* StringPin = CustomCreatePin(EGPD_Input, Schema->PC_String, FName("String"));
	StringPin->bNotConnectable = true;
}


#undef LOCTEXT_NAMESPACE
