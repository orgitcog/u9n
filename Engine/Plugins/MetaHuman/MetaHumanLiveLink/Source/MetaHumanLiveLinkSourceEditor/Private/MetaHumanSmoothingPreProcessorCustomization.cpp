// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanSmoothingPreProcessorCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "IDetailChildrenBuilder.h"

#include "MetaHumanSmoothingPreProcessor.h"

#define LOCTEXT_NAMESPACE "MetaHumanSmoothingPreProcessorCustomization"

void FMetaHumanSmoothingPreProcessorCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	InHeaderRow.NameContent()[
		SNew(STextBlock)
		.Text(LOCTEXT("SmoothingLabel", "MetaHuman Smoothing"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];
}

void FMetaHumanSmoothingPreProcessorCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	if (!InPropertyHandle->IsValidHandle())
	{
		return;
	}

	const TSharedRef<IPropertyHandle> ParametersProperty = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UMetaHumanSmoothingPreProcessor, Parameters)).ToSharedRef();
	InChildBuilder.AddProperty(ParametersProperty);
}

TSharedRef<IPropertyTypeCustomization> FMetaHumanSmoothingPreProcessorCustomization::MakeInstance()
{
	return MakeShareable(new FMetaHumanSmoothingPreProcessorCustomization);
}

#undef LOCTEXT_NAMESPACE
