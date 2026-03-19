// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSPin.h"

#define LOCTEXT_NAMESPACE "SDataflowPin"

void SDataflowPin::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	bIsPinInvalid = InArgs._IsPinInvalid.Get();
	PinColorOverride = InArgs._PinColorOverride.Get();
	bIsPinColorOverriden = InArgs._bIsPinColorOverriden.Get();

	const FText InvalidPinDisplayText = bIsPinInvalid.Get() ? LOCTEXT("DataflowOutputPinInvalidText", "*") : LOCTEXT("DataflowOutputPinValidText", " ");

	SGraphPin::Construct(SGraphPin::FArguments(), InPin);

	GetLabelAndValue()->AddSlot()
		.Padding(2.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([InvalidPinDisplayText]()
				{
					return InvalidPinDisplayText;
				})
		.MinDesiredWidth(5)
		];
}

FSlateColor SDataflowPin::GetPinColor() const
{
	const bool IsColorOverriden = bIsPinColorOverriden.Get();
	const FLinearColor ColorOverride = PinColorOverride.Get();

	UEdGraphPin* GraphPin = GetPinObj();
	if (GraphPin && !GraphPin->IsPendingKill())
	{
		if (bIsDiffHighlighted)
		{
			return FSlateColor(FLinearColor(0.9f, 0.2f, 0.15f));
		}
		if (GraphPin->bOrphanedPin)
		{
			return FSlateColor(FLinearColor::Red);
		}
		if (const UEdGraphSchema* Schema = GraphPin->GetSchema())
		{
			if (!GetPinObj()->GetOwningNode()->IsNodeEnabled() || GetPinObj()->GetOwningNode()->IsDisplayAsDisabledForced() || !IsEditingEnabled() || GetPinObj()->GetOwningNode()->IsNodeUnrelated())
			{
				if (IsColorOverriden)
				{
					return FSlateColor(ColorOverride);
				}

				return Schema->GetPinTypeColor(GraphPin->PinType) * FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);
			}

			if (IsColorOverriden)
			{
				return FSlateColor(ColorOverride);
			}

			return Schema->GetPinTypeColor(GraphPin->PinType) * PinColorModifier;
		}
	}

	return FLinearColor::White;
}
#undef LOCTEXT_NAMESPACE

