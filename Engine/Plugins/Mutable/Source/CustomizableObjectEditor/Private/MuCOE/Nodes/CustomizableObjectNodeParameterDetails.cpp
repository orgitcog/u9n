// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeParameterDetails.h"

#include "DetailLayoutBuilder.h"
#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeParameterDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeParameterDetails);
}

void FCustomizableObjectNodeParameterDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FCustomizableObjectNodeDetails::CustomizeDetails(DetailBuilder);
	
	DetailBuilder.HideProperty("ParameterName");
}