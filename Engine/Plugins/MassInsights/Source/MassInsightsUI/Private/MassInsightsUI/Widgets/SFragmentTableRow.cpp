// Copyright Epic Games, Inc. All Rights Reserved.

#include "SFragmentTableRow.h"

#include "Widgets/Text/STextBlock.h"

void MassInsights::SFragmentTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	TablePtr = InArgs._TablePtr;
	FragmentInfoPtr = InArgs._FragmentInfoPtr;
	SetEnabled(true);
	Super::Construct(Super::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> MassInsights::SFragmentTableRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	return SNew( STextBlock )
		.Text(FText::FromString( FragmentInfoPtr->Name ));
}
