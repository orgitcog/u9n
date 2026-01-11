// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureSample.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;
struct FPropertyChangedEvent;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeTextureSample::UCustomizableObjectNodeTextureSample()
	: Super()
{

}


void UCustomizableObjectNodeTextureSample::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	// Output
	
	UEdGraphPin* ColorPin = CustomCreatePinSimple(EGPD_Output, UEdGraphSchema_CustomizableObject::PC_Color);
	ColorPin->bDefaultValueIsIgnored = true;
	
	// Inputs
	
	UEdGraphPin* TexturePin = CustomCreatePinSimple(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Image);
	TexturePin->bDefaultValueIsIgnored = true;

	UEdGraphPin* XPin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Float,  FName(TEXT("X")));
	XPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* YPin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Float, FName(TEXT("Y")));
	YPin->bDefaultValueIsIgnored = true;
}

void UCustomizableObjectNodeTextureSample::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);

	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::UpdatedNodesPinName3)
	{
		if (UEdGraphPin* ColorOutput = FindPin(TEXT("Color"), EGPD_Output))
		{
			ColorOutput->PinFriendlyName = LOCTEXT("Color_Pin_Category", "Color");;
		}
		
		if (UEdGraphPin* ImageInput = FindPin(TEXT("Texture"), EGPD_Input))
		{
			ImageInput->PinFriendlyName = LOCTEXT("Image_Pin_Category", "Texture");
		}
	}
}


FText UCustomizableObjectNodeTextureSample::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Sample_Texture", "Sample Texture");
}


FLinearColor UCustomizableObjectNodeTextureSample::GetNodeTitleColor() const
{
	return UEdGraphSchema_CustomizableObject::GetPinTypeColor(UEdGraphSchema_CustomizableObject::PC_Color);
}


FText UCustomizableObjectNodeTextureSample::GetTooltipText() const
{
	return LOCTEXT("Texture_Sample_Tooltip","Get the color found in a texture at the targeted X and Y position (from 0.0 to 1.0, both included).");
}

#undef LOCTEXT_NAMESPACE

