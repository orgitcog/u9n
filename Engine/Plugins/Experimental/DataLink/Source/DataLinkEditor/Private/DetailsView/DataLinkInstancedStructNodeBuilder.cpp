// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLinkInstancedStructNodeBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

FDataLinkInstancedStructNodeBuilder::FDataLinkInstancedStructNodeBuilder(const TSharedRef<IPropertyHandle>& InInputDataElementHandle)
	: FInstancedStructDataDetails(InInputDataElementHandle)
	, InputDataElementHandle(InInputDataElementHandle)
{
}

void FDataLinkInstancedStructNodeBuilder::GenerateHeaderRowContent(FDetailWidgetRow& InNodeRow)
{
	FInstancedStructDataDetails::GenerateHeaderRowContent(InNodeRow);

	InNodeRow
		.ShouldAutoExpand(true)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(INVTEXT("["))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(3, 0, 0, 0)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(InputDataElementHandle->GetArrayIndex()))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(3, 0, 0, 0)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(INVTEXT("]"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(5, 0, 0, 0)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(InputDataElementHandle->GetPropertyDisplayName())
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

void FDataLinkInstancedStructNodeBuilder::Tick(float InDeltaTime)
{
	// TODO: Logic in FInstancedStructDataDetails::Tick calls EnumerateConstRawData which does not check that the underlying handle still has a valid property node
	if (InputDataElementHandle->IsValidHandle())
	{
		FInstancedStructDataDetails::Tick(InDeltaTime);
	}
}
