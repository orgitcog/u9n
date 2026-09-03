// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/SCustomizableObjectNodePin.h"

class SCustomizableObjectNodeStaticStringPin : public SCustomizableObjectNodePin
{
public:

	SLATE_BEGIN_ARGS(SCustomizableObjectNodeStaticStringPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	TSharedRef<SWidget>	GetDefaultValueWidget() override;

private:

	/** Return the value of the node */
	FText GetNodeStringValue() const;

	/** On Text Commited Callback */
	void OnTextCommited(const FText& InValue, ETextCommit::Type InCommitInfo);

	/** Returns the visibility of the pin widget */
	EVisibility GetWidgetVisibility() const;
};
