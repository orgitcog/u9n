// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodePassThroughTextureVariation.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


// UCustomizableObjectNodeVariation interface
FName UCustomizableObjectNodePassThroughTextureVariation::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_PassThroughImage;
}


void UCustomizableObjectNodePassThroughTextureVariation::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);

	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::UpdatedNodesPinName3)
	{
		if (UEdGraphPin* Pin = FindPin(TEXT("Texture")))
		{
			Pin->PinName = TEXT("PassThrough Texture");
			Pin->PinFriendlyName = LOCTEXT("PassThrough_Image_Pin_Category", "PassThrough Texture");
		}
	}
}


FText UCustomizableObjectNodePassThroughTextureVariation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("PassThrough_Texture_Variation", "PassThrough Texture Variation");
}

#undef LOCTEXT_NAMESPACE
