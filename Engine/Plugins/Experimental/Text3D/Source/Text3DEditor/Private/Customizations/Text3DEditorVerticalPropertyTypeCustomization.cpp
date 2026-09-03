// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/Text3DEditorVerticalPropertyTypeCustomization.h"

#include "DetailWidgetRow.h"
#include "Widgets/SText3DEditorVerticalAlignment.h"

TSharedRef<IPropertyTypeCustomization> FText3DEditorVerticalPropertyTypeCustomization::MakeInstance()
{
	return MakeShared<FText3DEditorVerticalPropertyTypeCustomization>();
}

void FText3DEditorVerticalPropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InRow, IPropertyTypeCustomizationUtils& InUtils)
{
	InRow.NameContent()
	[
		InPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SText3DEditorVerticalAlignment, InPropertyHandle)
	];
}

void FText3DEditorVerticalPropertyTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InBuilder, IPropertyTypeCustomizationUtils& InUtils)
{
	
}
