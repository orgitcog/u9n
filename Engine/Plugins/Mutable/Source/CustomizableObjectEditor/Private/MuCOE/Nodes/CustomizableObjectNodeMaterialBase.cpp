// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

FLinearColor UCustomizableObjectNodeMaterialBase::GetNodeTitleColor() const
{
	return UEdGraphSchema_CustomizableObject::GetPinTypeColor(UEdGraphSchema_CustomizableObject::PC_Material);
}
