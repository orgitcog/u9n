// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureParameter.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


bool UCustomizableObjectNodeTextureParameter::IsExperimental() const
{
	return true;
}


void UCustomizableObjectNodeTextureParameter::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);
	
	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::NodeTextureParameterDefaultToReferenceValue)
	{
		ReferenceValue = DefaultValue;
		DefaultValue = {};
	}
	else if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::UpdatedNodesPinName2)
	{
		if (UEdGraphPin* Pin = FindPin(TEXT("Value")))
		{
			Pin->PinName = TEXT("Texture");
			Pin->PinFriendlyName = LOCTEXT("Image_Pin_Category", "Texture");
		}
	}
}


FName UCustomizableObjectNodeTextureParameter::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Image;
}

#undef LOCTEXT_NAMESPACE

