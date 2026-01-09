// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureTransform.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void UCustomizableObjectNodeTextureTransform::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	UEdGraphPin* OutputPin = CustomCreatePinSimple(EGPD_Output, UEdGraphSchema_CustomizableObject::PC_Image);
	OutputPin->bDefaultValueIsIgnored = true;
	
	
	UEdGraphPin* ImagePin = CustomCreatePinSimple(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Image);
	BaseImagePinReference = FEdGraphPinReference(ImagePin);
	
	UEdGraphPin* OffsetXPin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Float, FName(TEXT("Offset X")));
	OffsetXPin->bDefaultValueIsIgnored = true;
	
	UEdGraphPin* OffsetYPin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Float, FName(TEXT("Offset Y")));
	OffsetYPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* ScaleXPin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Float, FName(TEXT("Scale X")));
	ScaleXPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* ScaleYPin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Float, FName(TEXT("Scale Y")));
	ScaleYPin->bDefaultValueIsIgnored = true;
	
	UEdGraphPin* RotationPin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Float, FName(TEXT("Rotation")));
	RotationPin->bDefaultValueIsIgnored = true;
}

void UCustomizableObjectNodeTextureTransform::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);

	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::UpdatedNodesPinName3)
	{
		if (UEdGraphPin* InputTexturePin = FindPin(TEXT("Base Texture"), EGPD_Input))
		{
			InputTexturePin->PinName = TEXT("Texture");
			InputTexturePin->PinFriendlyName = LOCTEXT("Image_Pin_Category", "Texture");
		}

		if (UEdGraphPin* OutputTexturePin = FindPin(TEXT("Texture"), EGPD_Output))
		{
			OutputTexturePin->PinFriendlyName = LOCTEXT("Image_Pin_Category", "Texture");
		}
	}
}

UEdGraphPin* UCustomizableObjectNodeTextureTransform::GetBaseImagePin() const
{
	return BaseImagePinReference.Get();
}

FText UCustomizableObjectNodeTextureTransform::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Texture_Transform", "Texture Transform");
}

FLinearColor UCustomizableObjectNodeTextureTransform::GetNodeTitleColor() const
{
	return UEdGraphSchema_CustomizableObject::GetPinTypeColor(UEdGraphSchema_CustomizableObject::PC_Image);
}

FText UCustomizableObjectNodeTextureTransform::GetTooltipText() const
{
	return LOCTEXT("Texture_Transform_Tooltip", 
			"Applies a linear transform, rotation and scale around the center of the image plus translation, "
			"to the content of Base Texture. Rotation is in the range [0 .. 1], 1 being full rotation, offset " 
			"and scale are in output image normalized coordinates with origin at the center of the image. " 
			"If Keep Aspect Ratio is set, an scaling factor preserving aspect ratio will be used as identity.");
}



#undef LOCTEXT_NAMESPACE
